# -*- coding: utf-8 -*-
"""check_ir_equiv.py — 验证计划 步骤3：对每个载体，`QCC_CODEGEN=ir` 与 legacy 的产物**逐文件 sha256 相同**。

与 `check_ir_j1.py` 的区别：J1 只看 replay 字节数；本判据直接对 `.obj` 整体取 sha256，
把「IR 路径 == 产物路径」这句话变成逐文件可复算的对照表。

用法: python scripts/check_ir_equiv.py [--cc qcc_zh_v1.exe]
退出码: 0 = 全部相同 / 1 = 有 DIFF
"""
import argparse
import hashlib
import io
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

CASES = [
    ("main", "int main(void) { return 42; }\n"),
    ("single_assign", "int f(void) { int x; x = 7; return x; }\n"),
    ("single_declinit", "int f(void) { int x = 7; return x; }\n"),
    ("while", "int f(int n) { int s = 0; while (n > 0) { s = s + n; n = n - 1; } return s; }\n"),
    ("for", "int f(int n) { int s = 0; for (int i = 0; i < n; i++) { s = s + i; } return s; }\n"),
    ("bitand", "int f(int a, int b) { return a & b; }\n"),
    ("shift_l", "int f(int a, int b) { return a << b; }\n"),
    ("shift_r", "int f(int a, int b) { return a >> b; }\n"),
    ("deref", "int f(int *p) { return *p; }\n"),
    ("localaddr", "int f(void) { int x = 7; int *p = &x; return *p; }\n"),
    ("logical_not", "int f(int a) { return !a; }\n"),
    ("bit_not", "int f(int a) { return ~a; }\n"),
    ("ternary", "int f(int a) { return a > 0 ? 1 : 2; }\n"),
    ("call_printf", "int printf(const char*, ...);\nint main(void) { printf(\"hi\\n\"); return 0; }\n"),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="qcc_zh_v1.exe")
    a = ap.parse_args()
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(repo, a.cc)
    if not os.path.exists(cc):
        print("[ERR] 缺件: %s" % cc)
        return 2
    work = tmp_dir("ir_equiv")
    bad = []
    print("%-16s %-18s %-18s %s" % ("载体", "legacy sha256", "IR sha256", "结论"))
    for tag, src in CASES:
        sp = os.path.join(work, tag + ".c")
        io.open(sp, "w", encoding="utf-8", newline="\n").write(src)
        shas = {}
        for mode, envx in (("base", {}), ("ir", {"QCC_CODEGEN": "ir"})):
            obj = os.path.join(work, "%s.%s.obj" % (tag, mode))
            env = dict(os.environ)
            for k in ("QCC_CODEGEN", "QCC_IR_STATS", "QCC_IR_FOLD", "QCC_OPT", "QCC_DUMP"):
                env.pop(k, None)
            env.update(envx)
            r = subprocess.run([cc, sp, "-c", "-o", obj], cwd=repo, capture_output=True, env=env)
            if r.returncode != 0 or not os.path.exists(obj):
                bad.append("%s: 编译失败 mode=%s rc=%d" % (tag, mode, r.returncode))
                shas[mode] = "ERR"
                continue
            shas[mode] = hashlib.sha256(io.open(obj, "rb").read()).hexdigest()[:16]
        ok = shas.get("base") == shas.get("ir")
        print("%-16s %-18s %-18s %s" % (tag, shas.get("base", "?"), shas.get("ir", "?"),
                                        "同" if ok else "**DIFF**"))
        if not ok:
            bad.append("%s: sha256 不同" % tag)
    print()
    if bad:
        print("[FAIL] check_ir_equiv：")
        for b in bad:
            print("  - " + b)
        return 1
    print("[通过 PASS] check_ir_equiv：%d 个载体 IR 产物 == legacy 产物（逐文件 sha256）" % len(CASES))
    return 0


if __name__ == "__main__":
    sys.exit(main())
