# -*- coding: utf-8 -*-
"""check_ir_printf_l1.py — J1 printf 子系统 L1 化的**产物逐字节**判据.

对每个探针：用 `--cc` 编 `-c`，记录 `.obj` 的 sha256 与 `.text` 大小；
与 `scripts/ir_printf_baseline.json`（改前基线）逐项比对。

用法:
  python scripts/check_ir_printf_l1.py --cc qcc_zh_v1.exe           # 判据
  python scripts/check_ir_printf_l1.py --cc qcc_zh_v1.exe --write-baseline  # 采基线（改前跑一次）
"""
import argparse
import hashlib
import io
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
HERE = os.path.dirname(os.path.abspath(__file__))
BASE = os.path.join(HERE, "ir_printf_baseline.json")

CASES = {
    "printf_lit": '#包含 <stdio.h>\n整 主(空) { printf("hi\\n"); 返 0; }\n',
    "printf_d": '#包含 <stdio.h>\n整 主(空) { printf("%d\\n", 42); 返 0; }\n',
    "printf_s": '#包含 <stdio.h>\n整 主(空) { printf("%s\\n", "x"); 返 0; }\n',
    "printf_f": '#包含 <stdio.h>\n整 主(空) { printf("%f\\n", 1.5); 返 0; }\n',
    "printf_mix": '#包含 <stdio.h>\n整 主(空) { printf("%08.3f|%+d|%x|%c\\n", 3.14159, -7, 255, 65); 返 0; }\n',
    "fprintf_e": '#包含 <stdio.h>\n整 主(空) { fprintf(stderr, "e\\n"); 返 0; }\n',
    "sprintf": '#包含 <stdio.h>\n整 主(空) { 字 b[64]; sprintf(b, "%d", 7); printf("%s\\n", b); 返 0; }\n',
    "snprintf": '#包含 <stdio.h>\n整 主(空) { 字 b[64]; snprintf(b, 8, "%d", 7); printf("%s\\n", b); 返 0; }\n',
    "printf_arg": '#包含 <stdio.h>\n整 f(整 x) { printf("%d\\n", x); 返 x; }\n整 主(空) { 返 f(3); }\n',
}


def sha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()


def one(cc, work, tag):
    src = os.path.join(work, tag + ".jy")
    obj = os.path.join(work, tag + ".obj")
    io.open(src, "w", encoding="utf-8-sig").write(CASES[tag])
    r = subprocess.run([cc, src, "-c", "-o", obj], capture_output=True, timeout=120)
    err = (r.stdout + r.stderr).decode("utf-8", "replace")
    if r.returncode != 0 or not os.path.exists(obj):
        return {"rc": r.returncode, "err": err.strip()[:120]}
    return {"rc": 0, "sha": sha(obj), "bytes": os.path.getsize(obj)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="qcc_zh_v1.exe")
    ap.add_argument("--write-baseline", action="store_true")
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(os.path.dirname(HERE), a.cc)
    if not os.path.exists(cc):
        print("[ERR] 缺件: %s" % cc)
        return 2
    work = tmp_dir("ir_printf")
    got = {t: one(cc, work, t) for t in CASES}
    if a.write_baseline:
        io.open(BASE, "w", encoding="utf-8", newline="\n").write(
            json.dumps(got, ensure_ascii=False, indent=1, sort_keys=True) + "\n")
        print("[BASE] written %s (%d cases)" % (BASE, len(got)))
        return 0
    if not os.path.exists(BASE):
        print("[ERR] 缺基线 %s（先 --write-baseline）" % BASE)
        return 2
    base = json.load(io.open(BASE, encoding="utf-8"))
    bad = []
    for t in CASES:
        b, g = base.get(t), got[t]
        ok = b and b.get("rc") == 0 and g.get("rc") == 0 and b.get("sha") == g.get("sha")
        print("%-12s obj=%s %s" % (t, (g.get("sha") or "ERR")[:12],
                                   "PASS" if ok else "**DIFF** base=%s got=%s" % ((b or {}).get("sha", "?")[:12], (g.get("sha") or "ERR")[:12])))
        if not ok:
            bad.append(t)
    if bad:
        print("\n[失败 FAIL] 产物字节变化: %s" % bad)
        return 1
    print("\n[通过 PASS] 9 个探针 .obj 逐字节与改前基线相同")
    return 0


if __name__ == "__main__":
    sys.exit(main())
