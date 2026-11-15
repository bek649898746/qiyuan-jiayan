# -*- coding: utf-8 -*-
"""check_ir_step4_link.py — IR 产物（-c）→ jyld 链接 → 运行（六类载体）。

判据：
  S1  每个用例：IR obj 与 legacy obj 逐字节相同
  S2  每个用例：IR exe 经 jyld 链接成功，运行 exit code == 期望
  S3  legacy exe 同样运行正确（对照）

用法: python scripts/check_ir_step4_link.py --cc qcc_zh_v1.exe
"""
import argparse, hashlib, io, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CASES = [
    ("value", "int main(void) { return 42; }\n", 42),
    ("slot", "int f(int d) { int x = 7; return x; }\nint main(void) { return f(0); }\n", 7),
    ("chain", "int f(int a) { return a + 1; }\nint main(void) { return f(41); }\n", 42),
    ("arg", "int h(int a) { return a; }\nint main(void) { return h(42); }\n", 42),
    ("ctrl", "int h(int a) { return a; }\nint f(int d) { if (h(1)) { return 1; } return 2; }\nint main(void) { return f(0); }\n", 1),
    ("assign", "int f(int d) { int x; x = 42; return x; }\nint main(void) { return f(0); }\n", 42),
]


def sha(b):
    return hashlib.sha256(b).hexdigest().upper()[:8]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default=os.path.join(ROOT, "qcc_zh_v1.exe"))
    ap.add_argument("--jyld", default=os.path.join(ROOT, "jyld.exe"))
    ap.add_argument("--rt", default=os.path.join(ROOT, "lib", "qcc_rt.obj"))
    a = ap.parse_args()
    if not os.path.exists(a.cc):
        print("[ERR] 缺件: %s" % a.cc); return 2
    d = tmp_dir("ir_step4")
    bad = []
    for tag, src, expect in CASES:
        c = os.path.join(d, tag + ".c"); io.open(c, "w", encoding="utf-8").write(src)
        bo = os.path.join(d, tag + "_b.obj"); io_ = os.path.join(d, tag + "_i.obj")
        be = os.path.join(d, tag + "_b.exe"); ie = os.path.join(d, tag + "_i.exe")
        eb = dict(os.environ); eb.pop("QCC_CODEGEN", None); eb.pop("QCC_IR_STATS", None)
        ei = dict(os.environ); ei["QCC_CODEGEN"] = "ir"; ei["QCC_IR_STATS"] = "1"
        rb = subprocess.run([a.cc, c, "-c", "-o", bo], capture_output=True, env=eb)
        ri = subprocess.run([a.cc, c, "-c", "-o", io_], capture_output=True, env=ei)
        if rb.returncode or ri.returncode:
            bad.append("%s: 编译失败 base=%d ir=%d" % (tag, rb.returncode, ri.returncode)); continue
        hb = sha(io.open(bo, "rb").read()); hi = sha(io.open(io_, "rb").read())
        if hb != hi:
            bad.append("%s: obj DIFF base=%s ir=%s" % (tag, hb, hi)); continue
        ok = True
        for obj, exe in ((bo, be), (io_, ie)):
            rl = subprocess.run([a.jyld, "-o", exe, obj, a.rt], capture_output=True)
            if rl.returncode != 0:
                bad.append("%s: jyld 失败 rc=%d" % (tag, rl.returncode)); ok = False
        if not ok:
            continue
        rr = subprocess.run([ie], capture_output=True)
        if rr.returncode != expect:
            bad.append("%s: IR exe exit=%d 期望=%d" % (tag, rr.returncode, expect))
        rrb = subprocess.run([be], capture_output=True)
        if rrb.returncode != expect:
            bad.append("%s: base exe exit=%d 期望=%d" % (tag, rrb.returncode, expect))
        print("  %-7s obj=%s ir_exit=%d base_exit=%d" % (tag, hb, rr.returncode, rrb.returncode))
    if bad:
        print("\n[FAIL] check_ir_step4_link:\n  - " + "\n  - ".join(bad)); return 1
    print("\n[通过 PASS] check_ir_step4_link：六类载体 obj 逐字节相同 · jyld 链接成功 · exit code 全对")
    return 0


if __name__ == "__main__":
    sys.exit(main())
