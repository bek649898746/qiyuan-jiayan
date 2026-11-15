# -*- coding: utf-8 -*-
"""check_colon_silent.py — ORD-20261108-151 判据：`若/否/循环/遍` 条件后的冒号必须**响亮报错**。

判据（施工单 J6 的核心诉求：语法的取舍可以商量，静默放行不行）：
  1. `feat_zh_colon_if_neg.c`    (`若 (假):`)     ⟹ rc != 0 且 stderr 含 A6112
  2. `feat_zh_colon_else_neg.c`  (`否:`)          ⟹ rc != 0 且 stderr 含 A6112
  3. `feat_zh_colon_while_neg.c` (`循环 (…):`)    ⟹ rc != 0 且 stderr 含 A6112
  4. `feat_zh_colon_for_neg.c`   (`遍 (…):`)      ⟹ rc != 0 且 stderr 含 A6112
  5. `feat_zh_colon_plain_ok.c`  (无冒号 对照)     ⟹ rc == 0 且输出 hit=100

用法: python scripts/check_colon_silent.py --cc qcc_zh_v1.exe
"""
import argparse
import io
import os
import subprocess
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

HERE = os.path.dirname(os.path.abspath(__file__))

NEG = [
    ("feat_zh_colon_if_neg.c", "若 (假):"),
    ("feat_zh_colon_else_neg.c", "否:"),
    ("feat_zh_colon_while_neg.c", "循环 (…):"),
    ("feat_zh_colon_for_neg.c", "遍 (…):"),
]
POS = ("feat_zh_colon_plain_ok.c", "hit=100")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="qcc_zh_v1.exe")
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(os.path.dirname(HERE), a.cc)
    if not os.path.exists(cc):
        print("[ERR] 缺件: %s" % cc)
        return 2
    d = tmp_dir("colon")
    bad = []
    for name, label in NEG:
        src = os.path.join(HERE, name)
        exe = os.path.join(d, name + ".exe")
        r = subprocess.run([cc, src, "-o", exe], capture_output=True, timeout=120)
        err = (r.stdout + r.stderr).decode("utf-8", "replace")
        ok = (r.returncode != 0) and ("A6112" in err)
        print("%-34s %-12s rc=%d  A6112=%s  %s" % (
            name, label, r.returncode, "A6112" in err, "PASS" if ok else "FAIL"))
        if not ok:
            bad.append("%s: rc=%d A6112=%s（应为 rc!=0 且含 A6112）"
                       % (name, r.returncode, "A6112" in err))
    name, want = POS
    src = os.path.join(HERE, name)
    exe = os.path.join(d, name + ".exe")
    r = subprocess.run([cc, src, "-o", exe], capture_output=True, timeout=120)
    if r.returncode != 0:
        bad.append("%s: 编译失败 rc=%d" % (name, r.returncode))
        print("%-34s 对照 rc=%d 构建失败" % (name, r.returncode))
    else:
        rr = subprocess.run([exe], capture_output=True, timeout=10)
        out = (rr.stdout + rr.stderr).decode("utf-8", "replace").strip()
        ok = (rr.returncode == 0) and (want in out)
        print("%-34s 对照 rc=%d  out=%r  %s" % (name, rr.returncode, out, "PASS" if ok else "FAIL"))
        if not ok:
            bad.append("%s: 对照 rc=%d out=%r（应 rc=0 且含 %s）" % (name, rr.returncode, out, want))
    if bad:
        print("\n[FAIL] check_colon_silent:\n  - " + "\n  - ".join(bad))
        return 1
    print("\n[通过 PASS] check_colon_silent：四处冒号全部 A6112 响亮拒绝，无冒号对照照常 hit=100")
    return 0


if __name__ == "__main__":
    sys.exit(main())
