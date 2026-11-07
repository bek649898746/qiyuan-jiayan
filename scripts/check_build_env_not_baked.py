#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""R896 判据：**构建期环境不得烘进产物**。

血案（2026-11-04）：`序置二()` 是编译期执行的入口序发射器，其中
`整 块量 = 堆块取();` 被**构建编译器**求值，`移即扩(1, 块量)` 直接把它写成
目标机器码的 imm32。当时 `堆块取()` 里有一行
`若 (取环境("QCC_SMALL_HEAP")) 返 0x10000000;` ⟹ 只要构建时 shell 带着
`QCC_SMALL_HEAP=1`，编出来的编译器竞技场就永久只剩 256 MiB；它再编
`srclib/opt_zh.jy` 时堆把竞技场吃穿 ⟹ 越顶写未提交页 ⟹ 0xC0000005。
（与「源码改了什么」无关，所以上一轮误判成「改一行就崩」。）

本判据只认实测：
  A. 同一份小源，**带** / **不带** `QCC_SMALL_HEAP=1` 各编一次 ⟹ 两个产物
     **逐字节相同**（若环境被烘进产物，prologue 的 imm32 会不同）。
  B. 产物里出现 `mov ecx,0x30000000`（768 MiB），且**不出现** `0x10000000`。
  C. 该编译器能编过 `srclib/opt_zh.jy`（原始失败现场），rc=0、有产物、两次一致。
  D. **出货路径**：`qcc_zh_seed.exe`（build_zh.ps1 的起跑编译器）在 `QCC_SMALL_HEAP=1`
     下编同一小源 ⟹ 产物逐字节相同；且它编 `srclib_jiayan/qcc_zh.jy` 得到的产物
     sha 必须等于 `build_zh.ps1` 的 `$expected`、入口 768 MiB。
     （skeptic 2026-11-04：只测 v1 会漏掉「旧 seed 仍可被毒化」这条真实路径。）

用法：
  python scripts/check_build_env_not_baked.py [--cc <编译器.exe>]

退出码：0 = 全绿；1 = 有判据红。
"""
import argparse
import hashlib
import os
import subprocess
import sys
from qcc_tmp import tmp_dir

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PROBE_SRC = """静 整 main(整 argc, 字 **argv) { 返 7; }
"""

MOV_768 = bytes.fromhex("b900000030")   # mov ecx, 0x30000000
MOV_256 = bytes.fromhex("b900000010")   # mov ecx, 0x10000000


def run_cc(cc, src, out, env_extra=None):
    env = os.environ.copy()
    env.pop("QCC_SMALL_HEAP", None)
    if env_extra:
        env.update(env_extra)
    try:
        p = subprocess.run([cc, src, "-o", out], cwd=ROOT, env=env,
                           capture_output=True, timeout=1800)
    except OSError as e:
        # fail-closed（skeptic #2 · E 组）：种子存在但不可执行（WinError 216 等）时
        # 原实现直接抛未捕获异常 —— 进程 rc≠0，但**没有可捕获的诊断标记**。
        return 127, "[FAIL] 种子不可执行: %s (%s)" % (cc, e)
    return p.returncode, (p.stdout + p.stderr).decode("utf-8", "replace")


def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def expected_hash():
    """从 build_zh.ps1 读 $expected（唯一事实源，避免硬编码）。"""
    p = os.path.join(ROOT, "scripts", "build_zh.ps1")
    try:
        src = open(p, encoding="utf-8", errors="replace").read()
    except OSError:
        return None
    import re
    m = re.search(r"^\$expected = '([0-9A-Fa-f]{8})'", src, re.M)
    return m.group(1).upper() if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default=os.path.join(ROOT, "qcc_zh_v1.exe"))
    ap.add_argument("--seed", default=os.path.join(ROOT, "qcc_zh_seed.exe"),
                    help="D 组用的起跑编译器（默认 = build_zh.ps1 用的 qcc_zh_seed.exe）")
    args = ap.parse_args()
    cc = args.cc
    if not os.path.exists(cc):
        print("[FAIL] 找不到编译器: %s" % cc)
        return 1

    print("== R896 判据：构建期环境不得烘进产物 ==")
    print("  编译器: %s" % os.path.basename(cc))

    tmp = tmp_dir("r896")
    probe = os.path.join(tmp, "probe.jy")
    open(probe, "w", encoding="utf-8", newline="\n").write(PROBE_SRC)

    a = os.path.join(tmp, "noenv.exe")
    b = os.path.join(tmp, "smallen.exe")

    rc_a, log_a = run_cc(cc, probe, a)
    if rc_a != 0 or not os.path.exists(a):
        print("[FAIL] A 组：无环境变量时编译失败 rc=%d\n%s" % (rc_a, log_a[-800:]))
        return 1

    rc_b, log_b = run_cc(cc, probe, b, {"QCC_SMALL_HEAP": "1"})
    if rc_b != 0 or not os.path.exists(b):
        print("[FAIL] A 组：QCC_SMALL_HEAP=1 时编译失败 rc=%d\n%s" % (rc_b, log_b[-800:]))
        return 1

    da, db = open(a, "rb").read(), open(b, "rb").read()
    if da != db:
        print("[FAIL] A 组：带/不带 QCC_SMALL_HEAP 的产物不同 ⟹ 构建期环境被烘进产物")
        print("       noenv  sha=%s size=%d" % (sha(a)[:16], len(da)))
        print("       small  sha=%s size=%d" % (sha(b)[:16], len(db)))
        if da.count(MOV_768) != db.count(MOV_768) or da.count(MOV_256) != db.count(MOV_256):
            print("       差异在最可疑处: mov ecx,0x30000000 %d→%d ; mov ecx,0x10000000 %d→%d"
                  % (da.count(MOV_768), db.count(MOV_768),
                     da.count(MOV_256), db.count(MOV_256)))
        return 1
    print("[PASS] A 组：带/不带 QCC_SMALL_HEAP 产物逐字节相同 (sha=%s)" % sha(a)[:16])

    if da.count(MOV_768) != 1 or da.count(MOV_256) != 0:
        print("[FAIL] B 组：入口立即数不是 768MiB（0x30000000=%d 次, 0x10000000=%d 次）"
              % (da.count(MOV_768), da.count(MOV_256)))
        return 1
    print("[PASS] B 组：入口竞技场恒 768 MiB")

    opt = os.path.join(ROOT, "srclib", "opt_zh.jy")
    if not os.path.exists(opt):
        print("[SKIP] C 组：没有 srclib/opt_zh.jy")
        return 0
    outs = []
    for i in (1, 2):
        o = os.path.join(tmp, "opt%d.exe" % i)
        rc, log = run_cc(cc, opt, o)
        if rc != 0 or not os.path.exists(o):
            print("[FAIL] C 组：编 srclib/opt_zh.jy 失败 (第 %d 次) rc=0x%08X\n%s"
                  % (i, rc & 0xFFFFFFFF, log[-800:]))
            return 1
        outs.append(o)
    if sha(outs[0]) != sha(outs[1]):
        print("[FAIL] C 组：两次产物不同 (%s vs %s)" % (sha(outs[0])[:16], sha(outs[1])[:16]))
        return 1
    print("[PASS] C 组：srclib/opt_zh.jy 编过 ×2，产物一致 (size=%d sha=%s)"
          % (os.path.getsize(outs[0]), sha(outs[0])[:16]))

    # ---- D 组：**出货路径** —— seed 才是 build_zh.ps1 的起跑编译器 ----
    # skeptic (2026-11-04): 只测 qcc_zh_v1.exe 会漏掉「旧 seed 仍可被毒化」这条真实路径。
    seed = args.seed
    if not os.path.exists(seed):
        print("[FAIL] D 组：没有 qcc_zh_seed.exe ⟹ 判据不完整，不得报绿")
        return 1

    # D1（秒级）：seed 在带/不带 QCC_SMALL_HEAP 下编同一小源 ⟹ 产物必须逐字节相同
    sa = os.path.join(tmp, "seed_noenv.exe")
    sb = os.path.join(tmp, "seed_small.exe")
    rc, log = run_cc(seed, probe, sa)
    if rc != 0 or not os.path.exists(sa):
        print("[FAIL] D1：seed 编小源失败 rc=0x%08X\n%s" % (rc & 0xFFFFFFFF, log[-800:]))
        return 1
    rc, log = run_cc(seed, probe, sb, {"QCC_SMALL_HEAP": "1"})
    if rc != 0 or not os.path.exists(sb):
        print("[FAIL] D1：seed 带 QCC_SMALL_HEAP=1 编小源失败 rc=0x%08X\n%s"
              % (rc & 0xFFFFFFFF, log[-800:]))
        return 1
    if sha(sa) != sha(sb):
        print("[FAIL] D1：seed 的产物随构建期环境变化 ⟹ seed 仍会把 env 烘进产物")
        print("        noenv sha=%s / small sha=%s" % (sha(sa)[:16], sha(sb)[:16]))
        return 1
    print("[PASS] D1：seed 带/不带 QCC_SMALL_HEAP 产物逐字节相同 (sha=%s)" % sha(sa)[:16])

    # D2（~50s，出货第一步）：seed + QCC_SMALL_HEAP=1 编主源 ⟹ 产物必须是 $expected、768MiB
    exp = expected_hash()
    if exp is None:
        print("[FAIL] D2：读不到 build_zh.ps1 的 $expected")
        return 1
    main_src = os.path.join(ROOT, "srclib_jiayan", "qcc_zh.jy")
    if not os.path.exists(main_src):
        print("[FAIL] D2：没有 srclib_jiayan/qcc_zh.jy ⟹ 判据不完整，不得报绿")
        return 1
    built = os.path.join(tmp, "seed_built.exe")
    rc, log = run_cc(seed, main_src, built, {"QCC_SMALL_HEAP": "1"})
    if rc != 0 or not os.path.exists(built):
        print("[FAIL] D2：seed 带 QCC_SMALL_HEAP=1 编主源失败 rc=0x%08X\n%s"
              % (rc & 0xFFFFFFFF, log[-800:]))
        return 1
    db = open(built, "rb").read()
    got = hashlib.sha256(db).hexdigest()[:8].upper()
    # 2026-11-06 ORD-20261106-VARARG（虾米）：`$expected` 记的是**自举收敛点**(v2/v3)，
    # 而 D2 跑的是「出货第一步」= seed 编主源 = v1。**行为变更轮次**（新编译器生成的
    # 代码形态变了 ⟹ 编译器自身源码里的 5+ 变参 sprintf 等调用按新逻辑重编）v1 != v2 是
    # 正常且必须的 —— 收敛判据是 v2 == v3（由 build_zh.ps1 / bootstrap 判据守，不在这里）。
    # 故接受两个合法值：got == $expected（v1==v2 的常规轮）或 got == sha(qcc_zh_v1.exe)
    # 且 v1 != $expected（确属行为变更轮）。产物仍必须是这两个之一 —— 守护不缩水。
    v1p = os.path.join(ROOT, "qcc_zh_v1.exe")
    _v1 = hashlib.sha256(open(v1p, "rb").read()).hexdigest()[:8].upper() if os.path.exists(v1p) else None
    _behav_change = bool(_v1) and _v1 != exp and got == _v1
    if got != exp and not _behav_change:
        print("[FAIL] D2：出货第一步产物被毒化 —— sha=%s != $expected=%s"
              % (got, exp))
        print("        mov ecx,0x30000000=%d, mov ecx,0x10000000=%d"
              % (db.count(MOV_768), db.count(MOV_256)))
        return 1
    if db.count(MOV_768) != 1 or db.count(MOV_256) != 0:
        print("[FAIL] D2：产物竞技场不是 768MiB（0x30000000=%d, 0x10000000=%d）"
              % (db.count(MOV_768), db.count(MOV_256)))
        return 1
    print("[PASS] D2：seed + QCC_SMALL_HEAP=1 编主源 ⟹ sha=%s == %s，且 768MiB" %
          (got, ("$expected" if got == exp else "qcc_zh_v1.exe（行为变更轮：v1 != v2，收敛由 v2==v3 守）")))

    print("[PASS] R896 全绿")
    return 0


if __name__ == "__main__":
    sys.exit(main())
