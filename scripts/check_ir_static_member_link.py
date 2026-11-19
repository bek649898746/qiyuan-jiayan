# -*- coding: utf-8 -*-
"""check_ir_static_member_link.py —— `-c` + jyld 的静态/全局结构体成员运行时判据

为什么单列一条（skeptic 2026-11-15）：
  `check_ir_byte_parity.py` 比较的是「IR 与 legacy 两条路径的 .o」——两者共享同一套
  发射代码，`-c` 的 `链接模式` 分支一旦丢字段偏移，两条会**同时**错，逐字节门仍全绿。
  本判据走 `-c` → `jyld` → 运行，并以 **C 语义真值**（不是两边互比）为准，专门守
  「静态结构体成员的读 / ++ / -- / 取址字段」这条链。

覆盖：读、后自减、前自增、`&t.b` 经指针改、`+=`。
用法: python scripts/check_ir_static_member_link.py [--cc qcc_zh_v1.exe]
"""
import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HDR = '#include <stdio.h>\nstruct T { int a; long b; };\n'

# (标签, 源, C 语义期望的 stdout)
CASES = [
    ("read", HDR + '''
static struct T t;
int main(void){ t.a=1; t.b=7; printf("%d\\n", (int)(t.a*10 + t.b)); return 0; }
''', "17"),

    ("postdec", HDR + '''
static struct T t;
int main(void){ t.a=1; t.b=7; t.b--; printf("%d\\n", (int)(t.a*10 + t.b)); return 0; }
''', "16"),

    ("preinc_global", HDR + '''
struct T g;
int main(void){ g.a=2; g.b=7; ++g.b; printf("%d\\n", (int)(g.a*10 + g.b)); return 0; }
''', "28"),

    ("addrof", HDR + '''
static struct T t;
int main(void){ long *p; t.a=1; t.b=17; p = &t.b; *p += 5; printf("%d\\n", (int)(t.a*10 + t.b)); return 0; }
''', "32"),

    ("plusassign", HDR + '''
static struct T t;
int main(void){ t.a=4; t.b=5; t.b += 2; printf("%d\\n", (int)(t.a*10 + t.b)); return 0; }
''', "47"),

    ("mixed_incdec", HDR + '''
struct T g;
int main(void){ g.a=10; g.b=3; g.a++; g.b--; ++g.a; printf("%d\\n", (int)(g.a*10 + g.b)); return 0; }
''', "122"),

    # 值上下文（skeptic 2026-11-15）：自增减的结果作为**值**（不是语句），
    # 初始化式 / 实参 / 二元操作数 三条路；`-c` 下曾是 0xC0000005（缺 REL32）。
    ("val_postinc", open(os.path.join(ROOT, "_evidence", "20261112_ir_longmarch", "probe_cases", "valctx_postinc.c"), encoding="utf-8").read(), "7"),

    ("val_preinc", open(os.path.join(ROOT, "_evidence", "20261112_ir_longmarch", "probe_cases", "valctx_preinc.c"), encoding="utf-8").read(), "8"),

    ("val_assign", open(os.path.join(ROOT, "_evidence", "20261112_ir_longmarch", "probe_cases", "valctx_assign.c"), encoding="utf-8").read(), "7"),

    ("val_binop", open(os.path.join(ROOT, "_evidence", "20261112_ir_longmarch", "probe_cases", "valctx_binop.c"), encoding="utf-8").read(), "8"),
]


def run_one(cc, jyld, rt, d, tag, src, expect):
    c = os.path.join(d, tag + ".c")
    with open(c, "w", encoding="utf-8") as f:
        f.write(src)
    obj = os.path.join(d, tag + ".obj")
    exe = os.path.join(d, tag + ".exe")
    env = dict(os.environ)
    env["QCC_CODEGEN"] = "ir"
    r = subprocess.run([cc, c, "-c", "-o", obj], capture_output=True, env=env, cwd=ROOT)
    if r.returncode != 0:
        return ("COMPILE-FAIL", (r.stdout or b"") + (r.stderr or b""))
    r = subprocess.run([jyld, "-o", exe, obj, rt], capture_output=True, cwd=ROOT)
    if r.returncode != 0:
        return ("LINK-FAIL", (r.stdout or b"") + (r.stderr or b""))
    r = subprocess.run([exe], capture_output=True, cwd=ROOT, timeout=60)
    out = (r.stdout or b"").decode("utf-8", "replace").strip()
    return (out, r.returncode)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default=os.path.join(ROOT, "qcc_zh_v1.exe"))
    ap.add_argument("--jyld", default=os.path.join(ROOT, "jyld.exe"))
    ap.add_argument("--rt", default=os.path.join(ROOT, "lib", "qcc_rt.obj"))
    a = ap.parse_args()
    for p in (a.cc, a.jyld, a.rt):
        if not os.path.exists(p):
            print("[ERR] 缺件: %s" % p)
            return 2
    d = tmp_dir("ir_static_member_link")
    bad = []
    for tag, src, expect in CASES:
        got, extra = run_one(a.cc, a.jyld, a.rt, d, tag, src, expect)
        ok = (got == expect)
        print("  %-14s got=%-6r expect=%-4r %s" % (tag, got, expect, "OK" if ok else "**MISMATCH**"))
        if not ok:
            bad.append("%s: got=%r expect=%r extra=%r" % (tag, got, expect, extra))
    if bad:
        print("\n[FAIL] check_ir_static_member_link（-c + jyld 运行时）:")
        for b in bad:
            print("  - " + b)
        return 1
    print("\n[通过 PASS] check_ir_static_member_link：%d 例 `-c`+jyld 运行时全部等于 C 语义真值" % len(CASES))
    return 0


if __name__ == "__main__":
    sys.exit(main())
