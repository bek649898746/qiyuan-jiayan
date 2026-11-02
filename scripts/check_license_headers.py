# -*- coding: utf-8 -*-
"""check_license_headers —— OpenSSF 文件头 subset 的可复算判据。

判据：
  1. 三个代表文件都含 `SPDX-License-Identifier: MIT` 与 `Copyright`；
  2. 锚文件抽样仍以 `// @EXPECTED` / `/* @EXPECTED` 行首可解析
     （防止「批量加版权头」误伤 run_tests_zh.py 的行首契约）。

用法: python scripts/check_license_headers.py
退出码: 0 = PASS / 1 = RED
"""
import glob
import io
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REQUIRED = [
    'srclib_jiayan/qcc_zh.jy',
    'scripts/build_zh.ps1',
    'scripts/run_tests_zh.py',
]
EXPECTED_HEAD = re.compile(r'^(?://|/\*)\s*@EXPECTED')


def main():
    bad = []
    for rel in REQUIRED:
        p = os.path.join(REPO, rel)
        if not os.path.exists(p):
            bad.append('缺文件: %s' % rel)
            continue
        t = io.open(p, encoding='utf-8', errors='replace').read()
        if 'SPDX-License-Identifier: MIT' not in t:
            bad.append('%s 缺 SPDX-License-Identifier: MIT' % rel)
        if 'Copyright' not in t:
            bad.append('%s 缺 Copyright' % rel)

    anchors = sorted(glob.glob(os.path.join(REPO, 'scripts', 'feat_zh_*.c')))[:20]
    hit = 0
    for p in anchors:
        first = io.open(p, encoding='utf-8', errors='replace').readline().lstrip('\ufeff').strip()
        if EXPECTED_HEAD.match(first):
            hit += 1
    if anchors and hit == 0:
        bad.append('锚文件抽样 0/%d 首行可解析 @EXPECTED（行首契约被破坏）' % len(anchors))

    print('代表文件 %d 份 · 锚抽样 %d 份（首行 @EXPECTED 命中 %d）'
          % (len(REQUIRED), len(anchors), hit))
    for b in bad:
        print('[RED] ' + b)
    print('== 文件头 subset 判据: %s ==' % ('PASS' if not bad else 'FAIL (%d 处)' % len(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
