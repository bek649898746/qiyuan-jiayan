# -*- coding: utf-8 -*-
"""check_struct_tag_kw.py — ORD-20261108-152 P1 判据：结构体标签撞词必须**响亮报错**。

  feat_zh_structtag_class_neg.c   `构 类 {…};`     ⟹ rc != 0 且含「结构体 标签名」
  feat_zh_structtag_struct_neg.c  `构 结构体 {…};` ⟹ rc != 0 且含「结构体 标签名」
  feat_zh_structtag_normal_ok.c   `构 圆 {…};`     ⟹ rc == 0 且输出 x = 1
  feat_zh_structtag_anon_ok.c     匿名 `构 {…}`     ⟹ rc == 0 且输出 x = 7

用法: python scripts/check_struct_tag_kw.py --cc qcc_zh_v1.exe
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
    ("feat_zh_structtag_class_neg.c", "类"),
    ("feat_zh_structtag_struct_neg.c", "结构体"),
]
POS = [
    ("feat_zh_structtag_normal_ok.c", "x = 1"),
    ("feat_zh_structtag_anon_ok.c", "x = 7"),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="qcc_zh_v1.exe")
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(os.path.dirname(HERE), a.cc)
    if not os.path.exists(cc):
        print("[ERR] 缺件: %s" % cc)
        return 2
    d = tmp_dir("structtag")
    bad = []
    for name, label in NEG:
        src = os.path.join(HERE, name)
        exe = os.path.join(d, name + ".exe")
        r = subprocess.run([cc, src, "-o", exe], capture_output=True, timeout=120)
        err = (r.stdout + r.stderr).decode("utf-8", "replace")
        msgok = ("结构体 标签名" in err) and (label in err)
        ok = (r.returncode != 0) and msgok
        print("%-34s rc=%d  msg=%s  %s" % (name, r.returncode, msgok, "PASS" if ok else "FAIL"))
        if not ok:
            bad.append("%s: rc=%d msg=%s（应 rc!=0 且报「结构体 标签名」含 '%s'）"
                       % (name, r.returncode, msgok, label))
    for name, want in POS:
        src = os.path.join(HERE, name)
        exe = os.path.join(d, name + ".exe")
        r = subprocess.run([cc, src, "-o", exe], capture_output=True, timeout=120)
        if r.returncode != 0:
            bad.append("%s: 编译失败 rc=%d" % (name, r.returncode))
            print("%-34s rc=%d 构建失败" % (name, r.returncode))
            continue
        rr = subprocess.run([exe], capture_output=True, timeout=10)
        out = (rr.stdout + rr.stderr).decode("utf-8", "replace").strip()
        ok = (rr.returncode == 0) and (want in out)
        print("%-34s rc=%d  out=%r  %s" % (name, rr.returncode, out, "PASS" if ok else "FAIL"))
        if not ok:
            bad.append("%s: rc=%d out=%r（应 rc=0 且含 %s）" % (name, rr.returncode, out, want))
    if bad:
        print("\n[FAIL] check_struct_tag_kw:\n  - " + "\n  - ".join(bad))
        return 1
    print("\n[通过 PASS] check_struct_tag_kw：撞词标签 `类`/`结构体` 全部响亮报错，正常/匿名结构体不回归")
    return 0


if __name__ == "__main__":
    sys.exit(main())
