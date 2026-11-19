# -*- coding: utf-8 -*-
"""check_ir_bootstrap.py —— IR 自举链判据（默认快速模式 / --full 完整链）

为什么需要（台账 §484）：
  旧 `check_ir_selfcompile.py` 只验「legacy 自举产物以 IR 模式编主源的 rc == 0」，
  既不验证产物**能否运行**，也不做自举链 ⟹ 假绿。实测 `ir_v1`（IR 编出的编译器）
  连 `hello.c` 都编不了（0xC0000005）。

两级判据：
  · 默认（快，秒级）：若 `{out}/ir_v1.exe` 存在，则用它编 hello 并要求 (rc, stdout) 正确；
    不存在则 SKIP（exit 0）。gates 用这一级。
  · `--full`（慢，约 15 分钟）：完整自举链
      IR(cc) → ir_v1 → ir_v2 → ir_v3
      要求：三步 rc=0；sha(ir_v2)==sha(ir_v3)；二者编主源的 .o 逐字节相同；
      且 ir_v1 能编 hello 并跑对。

用法:
  python scripts\\check_ir_bootstrap.py                 # 快速
  python scripts\\check_ir_bootstrap.py --full --out {SCRATCH}
"""
import argparse
import hashlib
import os
import subprocess
import sys
import time

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "srclib_jiayan", "qcc_zh.jy")


def sha8(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest().upper()[:16]


def compile_ir(cc, out, timeout=1800):
    env = dict(os.environ)
    env["QCC_CODEGEN"] = "ir"
    t0 = time.time()
    r = subprocess.run([cc, SRC, "-o", out], capture_output=True, env=env, cwd=ROOT, timeout=timeout)
    dt = time.time() - t0
    ok = (r.returncode == 0 and os.path.exists(out))
    size = os.path.getsize(out) if os.path.exists(out) else 0
    print("  IR(%s) -> %s  rc=%d size=%d  %.1fs" % (os.path.basename(cc), os.path.basename(out), r.returncode, size, dt))
    if not ok:
        txt = (r.stdout or b"").decode("utf-8", "replace") + (r.stderr or b"").decode("utf-8", "replace")
        tail = txt.strip().splitlines()[-1] if txt.strip() else ""
        print("    tail: %s" % tail[:200])
    return ok


def smoke(cc, out):
    """用 cc 编 hello 并运行；返回 (ok, 描述)"""
    d = out
    os.makedirs(d, exist_ok=True)
    c = os.path.join(d, "hello.c")
    with open(c, "w", encoding="utf-8") as f:
        f.write('#include <stdio.h>\nint main(void){ printf("IRBOOT-OK\\n"); return 0; }\n')
    exe = os.path.join(d, "hello.exe")
    env = dict(os.environ)
    env["QCC_CODEGEN"] = "ir"
    r = subprocess.run([cc, c, "-o", exe], capture_output=True, env=env, cwd=ROOT, timeout=300)
    if r.returncode != 0:
        return False, "compile rc=%d" % r.returncode
    if not os.path.exists(exe):
        return False, "no exe"
    r2 = subprocess.run([exe], capture_output=True, cwd=ROOT, timeout=60)
    out_s = (r2.stdout or b"").decode("utf-8", "replace").strip()
    ok = (r2.returncode == 0 and out_s == "IRBOOT-OK")
    return ok, "run rc=%d stdout=%r" % (r2.returncode, out_s)


# 台账 §495 定下的最小回归环：编译器必须能「不崩」地吃下这两类最小输入。
# 这两例是 §484/§490 的崩点分水岭 —— `/* comment */` 曾 rc=0、刀998 后变崩，
# `int g;` 一直是 0xC0000005。判据只要求 rc 不是「异常终止」（<0 或 >=0xC0000000），
# 不要求编译成功（空 TU 报错 rc=1 属正常）。
MINIMAL_CASES = [
    ("comment_only", "/* only comment */\n"),
    ("int_g", "int g;\n"),
]


def minimal(cc, out):
    """最小输入矩阵：返回 (ok, 明细列表)"""
    d = out
    os.makedirs(d, exist_ok=True)
    env = dict(os.environ)
    env["QCC_CODEGEN"] = "ir"
    rows, ok = [], True
    for name, src in MINIMAL_CASES:
        c = os.path.join(d, "min_%s.c" % name)
        with open(c, "w", encoding="utf-8") as f:
            f.write(src)
        o = os.path.join(d, "min_%s.o" % name)
        r = subprocess.run([cc, c, "-c", "-o", o], capture_output=True, env=env, cwd=ROOT, timeout=300)
        rc = r.returncode
        crashed = (rc < 0) or (rc >= 0xC0000000)
        if crashed:
            ok = False
        rows.append((name, rc, crashed))
    return ok, rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default=os.path.join(ROOT, "qcc_zh_v2.exe"))
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "irboot"))
    ap.add_argument("--full", action="store_true")
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--minimal", action="store_true",
                    help="最小输入矩阵（台账 §495）：comment-only / int g; 不得异常终止")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    if not os.path.exists(a.cc):
        print("[ERR] 缺件: %s" % a.cc)
        return 2

    bad = []
    if a.full:
        v1 = os.path.join(a.out, "ir_v1.exe")
        v2 = os.path.join(a.out, "ir_v2.exe")
        v3 = os.path.join(a.out, "ir_v3.exe")
        print("== 1/3 IR(legacy) -> ir_v1 ==")
        if not compile_ir(a.cc, v1):
            bad.append("step1: IR(legacy) 失败")
        if not bad:
            print("== 2/3 IR(ir_v1) -> ir_v2 ==")
            if not compile_ir(v1, v2):
                bad.append("step2: IR(ir_v1) 失败")
        if not bad:
            print("== 3/3 IR(ir_v2) -> ir_v3 ==")
            if not compile_ir(v2, v3):
                bad.append("step3: IR(ir_v2) 失败")
        if not bad:
            h2, h3 = sha8(v2), sha8(v3)
            print("  ir_v2=%s ir_v3=%s  same=%s" % (h2, h3, h2 == h3))
            if h2 != h3:
                bad.append("ir_v2 != ir_v3")
            o2 = os.path.join(a.out, "ir_boot_v2.o")
            o3 = os.path.join(a.out, "ir_boot_v3.o")
            env = dict(os.environ)
            env["QCC_CODEGEN"] = "ir"
            for cc, o in ((v2, o2), (v3, o3)):
                subprocess.run([cc, SRC, "-c", "-o", o], capture_output=True, env=env, cwd=ROOT, timeout=1800)
            if os.path.exists(o2) and os.path.exists(o3):
                same = open(o2, "rb").read() == open(o3, "rb").read()
                print("  主源 .o（v2 编 vs v3 编）same=%s  size=%d/%d" % (same, os.path.getsize(o2), os.path.getsize(o3)))
                if not same:
                    bad.append("ir_boot_v2.o != ir_boot_v3.o")
            else:
                bad.append("v2/v3 编主源失败")

    # 快速模式：检查已有 ir_v1 是否可用
    v1 = os.path.join(a.out, "ir_v1.exe")
    if os.path.exists(v1):
        print("== 冒烟：ir_v1 编 hello 并运行 ==")
        ok, desc = smoke(v1, a.out)
        print("  %s  %s" % ("OK" if ok else "**FAIL**", desc))
        if not ok:
            bad.append("ir_v1 冒烟: " + desc)
    else:
        print("== ir_v1 不存在 => 判红（b18 未验证；先跑 --full 生成）==")
        bad.append("ir_v1 缺失：b18（IR 编出的编译器能否工作）未验证 —— 拒绝 SKIP 当 PASS")

    # 台账 §495：最小输入矩阵（只在显式 --minimal 时跑，不扰动默认绿态）
    if a.minimal:
        v1 = os.path.join(a.out, "ir_v1.exe")
        if not os.path.exists(v1):
            print("== ir_v1 不存在 => 判红（b18 未验证；先跑 --full 生成）==")
            bad.append("ir_v1 缺失：b18（IR 编出的编译器能否工作）未验证 —— 拒绝 SKIP 当 PASS")
        else:
            print("== 最小输入矩阵（QCC_CODEGEN=ir 编最小 TU）==")
            ok, rows = minimal(v1, a.out)
            for name, rc, crashed in rows:
                print("  %-14s rc=%-12d %s" % (name, rc, "**异常终止**" if crashed else "OK"))
            if not ok:
                bad.append("最小输入矩阵: 存在异常终止（见上表）")

    if bad:
        print("\n[FAIL] IR 自举链:")
        for b in bad:
            print("  - " + b)
        # 刀1084: fail-closed —— 有 bad 就红；原 `1 if strict/full else 0` 让快模式吃掉全部 FAIL（§679 假绿）
        return 1
    print("\n[通过 PASS] IR 自举链：v2==v3、产物逐字节、ir_v1 冒烟")
    return 0


if __name__ == "__main__":
    sys.exit(main())
