# -*- coding: utf-8 -*-
"""build_old_cc.py —— 用**历史版本的主源**造一台对照编译器（J7 A/B 用）。

用法: python scripts/build_old_cc.py [--rev HEAD~1] [--seed qcc_zh_seed.exe] [--out build/pre_ord158.exe]
原理: `git show <rev>:srclib_jiayan/qcc_zh.jy` 原样落盘（含 BOM）→ 用当前种子编译。
"""
import argparse
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rev", default="HEAD~1")
    ap.add_argument("--seed", default="qcc_zh_seed.exe")
    ap.add_argument("--out", default=os.path.join("build", "pre_ord158.exe"))
    a = ap.parse_args()
    seed = a.seed if os.path.isabs(a.seed) else os.path.join(REPO, a.seed)
    out = a.out if os.path.isabs(a.out) else os.path.join(REPO, a.out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    src = os.path.join(os.path.dirname(out), "_old_%s.jy" % a.rev.replace("/", "_").replace("~", "up"))
    r = subprocess.run(["git", "show", "%s:srclib_jiayan/qcc_zh.jy" % a.rev], cwd=REPO, capture_output=True)
    if r.returncode != 0:
        print("[ERR] git show 失败: %s" % a.rev)
        return 2
    io.open(src, "wb").write(r.stdout)
    c = subprocess.run([seed, src, "-o", out], cwd=REPO, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    if c.returncode != 0 or not os.path.exists(out):
        print("[ERR] 旧编译器 编译失败 rc=%d\n%s" % (c.returncode, (c.stdout + c.stderr)[-300:]))
        return 1
    print("OK: %s <- %s:%s（%d 字节）" % (os.path.relpath(out, REPO), a.rev, "srclib_jiayan/qcc_zh.jy",
                                          os.path.getsize(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
