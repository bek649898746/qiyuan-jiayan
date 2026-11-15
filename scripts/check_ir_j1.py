# -*- coding: utf-8 -*-
"""check_ir_j1.py — 裁定 B 的 J1 判据：`全 .text 由 L1 发出`。

判据（唯一）：
    对每个载体，编译器在 IR 模式输出的 `[IR-STATS] fn replay=R 函数=<名>`
    中的 R 必须**等于该 `.text` 段的字节数**。
    R = 第二遍从 L1 重放的真实落盘字节数（`重放计` 在函数首尾的差）。

为什么用 replay 而不是 muted：
    `muted` 是第一遍「尝试」的字节数，但部分静默块（如赋值源值）没有独立统计行，
    且 `中发` 缓冲在静默期外的 `中刷` 落盘 ⟹ muted 会少算。
    `replay` 是重放时真正经过 `发字节` 的字节数，与 `.text` 同口径。

缺口表 KNOWN_GAP 是**诚实登记**：值为 0 表示该载体已达成 J1。
新增载体前先在这里登记，不允许悄悄加进 PASS 集合。

用法: python scripts/check_ir_j1.py --cc qcc_zh_v1.exe
"""
import argparse
import hashlib
import io
import os
import re
import subprocess
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

CASES = [
    ("main", "int main(void) { return 42; }\n", 0),
    ("single_assign", "int f(void) { int x; x = 7; return x; }\n", 0),
    ("single_declinit", "int f(void) { int x = 7; return x; }\n", 0),
    ("while", "int f(int n) { int s = 0; while (n > 0) { s = s + n; n = n - 1; } return s; }\n", 0),
    ("for", "int f(int n) { int s = 0; for (int i = 0; i < n; i++) { s = s + i; } return s; }\n", 0),
    ("bitand", "int f(int a, int b) { return a & b; }\n", 0),
    ("shift_l", "int f(int a, int b) { return a << b; }\n", 0),
    ("shift_r", "int f(int a, int b) { return a >> b; }\n", 0),
    ("deref", "int f(int *p) { return *p; }\n", 0),
    ("localaddr", "int f(void) { int x = 7; int *p = &x; return *p; }\n", 0),
    ("logical_not", "int f(int a) { return !a; }\n", 0),
    ("bit_not", "int f(int a) { return ~a; }\n", 0),
    ("ternary", "int f(int a) { return a > 0 ? 1 : 2; }\n", 0),
    ("call_printf", "int printf(const char*, ...);\nint main(void) { printf(\"hi\\n\"); return 0; }\n", 0),
]

RE_FN = re.compile(r"\[IR-STATS\] fn muted=(\d+) replay=(\d+) 函数=(\S+)")


def coff_text(path):
    b = io.open(path, "rb").read()
    nsec = int.from_bytes(b[2:4], "little")
    total = 0
    parts = []
    for i in range(nsec):
        off = 20 + i * 40
        name = b[off:off + 8].rstrip(b"\x00")
        if name.startswith(b".text"):
            size = int.from_bytes(b[off + 16:off + 20], "little")
            ptr = int.from_bytes(b[off + 20:off + 24], "little")
            total += size
            parts.append(b[ptr:ptr + size])
    return total, hashlib.sha256(b"".join(parts)).hexdigest()[:16]


def run_case(cc, tag, src, known_gap, work):
    sp = os.path.join(work, tag + ".c")
    io.open(sp, "w", encoding="utf-8").write(src)
    ob = os.path.join(work, tag + ".base.obj")
    oi = os.path.join(work, tag + ".ir.obj")
    eb = dict(os.environ)
    eb.pop("QCC_CODEGEN", None)
    eb.pop("QCC_IR_STATS", None)
    ei = dict(os.environ)
    ei["QCC_CODEGEN"] = "ir"
    ei["QCC_IR_STATS"] = "1"
    rb = subprocess.run([cc, sp, "-c", "-o", ob], capture_output=True, env=eb, timeout=180)
    ri = subprocess.run([cc, sp, "-c", "-o", oi], capture_output=True, env=ei, timeout=180)
    if rb.returncode or ri.returncode:
        return ["%s: 编译失败 base=%d ir=%d" % (tag, rb.returncode, ri.returncode)], None
    err = (ri.stdout + ri.stderr).decode("utf-8", "replace")
    fns = RE_FN.findall(err)
    if not fns:
        return ["%s: 缺 [IR-STATS] fn 行" % tag], None
    replay = sum(int(r) for _, r, _ in fns)
    tb, hb = coff_text(ob)
    ti, hi = coff_text(oi)
    bad = []
    if hb != hi:
        bad.append("%s: 产物 DIFF base=%s ir=%s" % (tag, hb, hi))
    gap = ti - replay
    if gap != known_gap:
        bad.append("%s: J1 缺口 %d B，登记值 %d B（不一致 ⟹ 有回归或旧登记过期）" %
                   (tag, gap, known_gap))
    return bad, (replay, ti, gap, hb, hi, [f[2] for f in fns])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="qcc_zh_v1.exe")
    a = ap.parse_args()
    if not os.path.exists(a.cc):
        print("[ERR] 缺件: %s" % a.cc)
        return 2
    work = tmp_dir("ir_j1")
    bad_all = []
    open_gap = []
    for tag, src, known in CASES:
        bad, info = run_case(a.cc, tag, src, known, work)
        bad_all += bad
        if info:
            replay, text, gap, hb, hi, names = info
            mark = "OK" if gap == 0 else "缺口 %d B" % gap
            print("%-16s fn=%s replay=%d  .text=%d  %s  %s" % (
                tag, ",".join(names), replay, text, mark,
                "EQUAL" if hb == hi else "DIFF"))
            if gap != 0:
                open_gap.append((tag, gap))
    print()
    if bad_all:
        print("[FAIL] check_ir_j1（缺口与登记不一致 ⟹ 有回归）:")
        for b in bad_all:
            print("  - " + b)
        return 1
    if open_gap:
        print("[未达成] J1「全 .text 由 L1 发出」还差：")
        for tag, gap in open_gap:
            print("  - %-16s %d B" % (tag, gap))
        print("  （缺口已逐项登记在 KNOWN_GAP；清到 0 才可判 PASS）")
        return 1
    print("[通过 PASS] check_ir_j1：全部载体 `fn replay == len(.text)` —— 全 .text 由 L1 发出")
    return 0


if __name__ == "__main__":
    sys.exit(main())
