# -*- coding: utf-8 -*-
"""check_opt_ir_fold.py — 优化器「接上产物路径」判据（常量折叠 = 真 IR pass，可开关）。

判据（五条）：
  1. `QCC_CODEGEN=ir` 的产物与 legacy **逐字节相同**（IR 路径 = 产物路径）；
  2. `QCC_IR_FOLD=1`（重放前折叠 L1）使产物**变小**且字节**改变** ⟹ pass 真在产物路径上；
  3. `QCC_DUMP=ir` 的 **pass 表**能区分开/关：开时出现 `末丙=9102` 且 `结果数>0`，关时无该 pass；
  4. 关闭 pass（不给 `QCC_IR_FOLD`，或 `QCC_OPT=none` / `QCC_OPT=legacy`）⟹ 产物**逐字节回到** `ir` 基线；
  5. 记录 **墙钟 + CPU 时间**（psutil，子进程 user+system）与字节数 —— 不只报体积。
     **收益口径（大哥 2026-11-09 裁定）**：认「产物体积收益 + 如实记录耗时」；编译器固定开销
     （读源/预置头 ≈450 ms）远大于折叠省下的发射，故时间的**中/微负**是预期结果，不得谎报为收益。

用法: python scripts/check_opt_ir_fold.py [--cc qcc_zh_v1.exe] [--reps 5]
退出码: 0 = PASS / 1 = RED
"""
import argparse
import hashlib
import io
import os
import re
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

BENCH = """int f(int a) {
  int x = 1 + 2;
  int y = x * 3 + 4;
  if (a > 0) { return y + (2 * 5); }
  return x - 1;
}
int g(int a) {
  int p = 6 * 7;
  int q = p - (3 + 4);
  return a + q + (8 * 2);
}
int main(void) { return f(3) + g(4) + (6 * 7); }
"""

MODES = [
    ("legacy", {}, False),
    ("ir", {"QCC_CODEGEN": "ir"}, False),
    ("ir+fold", {"QCC_CODEGEN": "ir", "QCC_IR_FOLD": "1"}, True),
    ("ir+fold+OPT=none", {"QCC_CODEGEN": "ir", "QCC_IR_FOLD": "1", "QCC_OPT": "none"}, False),
    ("ir+fold+OPT=legacy", {"QCC_CODEGEN": "ir", "QCC_IR_FOLD": "1", "QCC_OPT": "legacy"}, False),
]


def text_size(path):
    b = io.open(path, "rb").read()
    nsec = int.from_bytes(b[2:4], "little")
    total = 0
    for i in range(nsec):
        off = 20 + i * 40
        name = b[off:off + 8].rstrip(b"\x00")
        if name.startswith(b".text"):
            total += int.from_bytes(b[off + 16:off + 20], "little")
    return total


def run_once(cc, repo, src, obj, extra, dump=False):
    env = dict(os.environ)
    for k in ("QCC_CODEGEN", "QCC_IR_STATS", "QCC_IR_FOLD", "QCC_OPT", "QCC_DUMP"):
        env.pop(k, None)
    env.update(extra)
    if dump:
        env["QCC_DUMP"] = "ir"
    t0 = time.perf_counter()
    p = subprocess.Popen([cc, src, "-c", "-o", obj], cwd=repo, env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    cpu = [0.0]

    def sampler():   # CPU 时间：子进程存活期 轮询 psutil（退出后拿不到）
        try:
            import psutil
            proc = psutil.Process(p.pid)
            while p.poll() is None:
                try:
                    t = proc.cpu_times()
                    cpu[0] = max(cpu[0], t.user + t.system)
                except Exception:
                    return
                time.sleep(0.005)
        except Exception:
            return

    th = threading.Thread(target=sampler, daemon=True)
    th.start()
    try:
        out_b, _ = p.communicate(timeout=600)
    except Exception:
        p.kill()
        out_b = b""
    th.join(timeout=2)
    out = (out_b or b"").decode("utf-8", "replace")
    wall = time.perf_counter() - t0
    return p.returncode, out, wall, cpu[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="qcc_zh_v1.exe")
    ap.add_argument("--reps", type=int, default=5)
    a = ap.parse_args()
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(repo, a.cc)
    if not os.path.exists(cc):
        print("[ERR] 缺件: %s" % cc)
        return 2
    work = tmp_dir("opt_ir_fold")
    src = os.path.join(work, "bench.c")
    io.open(src, "w", encoding="utf-8", newline="\n").write(BENCH)

    res = {}
    print("%-20s %8s %-18s %10s %10s" % ("模式", "text(B)", "sha", "墙钟中位ms", "CPU中位ms"))
    for tag, extra, _want in MODES:
        obj = os.path.join(work, "p_%s.obj" % tag.replace("+", "_").replace("=", "_"))
        walls, cpus = [], []
        for _ in range(a.reps):
            rc, out, w, c = run_once(cc, repo, src, obj, extra)
            if rc != 0:
                print("[RED] %s 编译失败 rc=%d %s" % (tag, rc, out[:200]))
                return 1
            walls.append(w * 1000)
            cpus.append(c * 1000)
        walls.sort(); cpus.sort()
        sha = hashlib.sha256(io.open(obj, "rb").read()).hexdigest()[:16]
        size = text_size(obj)
        res[tag] = (sha, size, walls[len(walls) // 2], cpus[len(cpus) // 2])
        print("%-20s %8d %-18s %10.1f %10.1f" % (tag, size, sha, res[tag][2], res[tag][3]))

    # 3) QCC_DUMP=ir 的 pass 表：开 ⟹ 9102 且 结果数>0；关 ⟹ 无 9102
    rc, dump_on, _w, _c = run_once(cc, repo, src, os.path.join(work, "d_on.obj"),
                                   {"QCC_CODEGEN": "ir", "QCC_IR_FOLD": "1"}, dump=True)
    rc2, dump_off, _w2, _c2 = run_once(cc, repo, src, os.path.join(work, "d_off.obj"),
                                       {"QCC_CODEGEN": "ir"}, dump=True)
    m_on = re.search(r"pass\[\d+\] 末丙=9102 结果数=(\d+)", dump_on)
    has_off = re.search(r"末丙=9102", dump_off) is not None

    bad = []
    if res["legacy"][0] != res["ir"][0]:
        bad.append("IR 产物 != legacy 产物（IR 路径 与 产物路径 不等价）")
    if res["ir+fold"][0] == res["ir"][0]:
        bad.append("QCC_IR_FOLD=1 产物 与 ir 基线 相同 ⟹ pass 没进产物路径")
    if res["ir+fold"][1] >= res["ir"][1]:
        bad.append("QCC_IR_FOLD=1 未省字节 (%d -> %d)" % (res["ir"][1], res["ir+fold"][1]))
    if not m_on or int(m_on.group(1)) <= 0:
        bad.append("QCC_DUMP=ir 在 fold 开启时未见 `pass[..] 末丙=9102 结果数>0`（产物路径 pass 无记录）")
    if has_off:
        bad.append("QCC_DUMP=ir 在 fold 关闭时仍出现 9102 ⟹ 开关不可区分")
    for off in ("ir+fold+OPT=none", "ir+fold+OPT=legacy"):
        if res[off][0] != res["ir"][0]:
            bad.append("%s 未回到 ir 基线（逐字节）" % off)

    print()
    print("省字节 = %d B (%.1f%%)" % (res["ir"][1] - res["ir+fold"][1],
                                      100.0 * (res["ir"][1] - res["ir+fold"][1]) / max(1, res["ir"][1])))
    print("墙钟 ir vs ir+fold = %.1f vs %.1f ms；CPU ir vs ir+fold = %.1f vs %.1f ms（同源 %d 连跑中位）"
          % (res["ir"][2], res["ir+fold"][2], res["ir"][3], res["ir+fold"][3], a.reps))
    print("pass 表: fold 开 -> %s | fold 关 -> %s"
          % ("末丙=9102 结果数=%s" % m_on.group(1) if m_on else "缺",
             "有 9102（错）" if has_off else "无 9102（正确）"))
    if bad:
        print("[RED] check_opt_ir_fold：")
        for b in bad:
            print("  - " + b)
        return 1
    print("[通过 PASS] check_opt_ir_fold：pass 进产物路径 · dump 可区分开关 · 关掉逐字节回基线 · 墙钟+CPU 已记")
    return 0


if __name__ == "__main__":
    sys.exit(main())
