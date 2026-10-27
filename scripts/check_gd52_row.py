# -*- coding: utf-8 -*-
"""check_gd52_row —— 施工单《G-D52_PERF 改 blocked》的台账状态闸。

判据（不是速度判据，速度监视在 check_gd52_margin.py）：
  ① `_TASKLIST.json` 的 `G-D52-PERF` 行 status == `blocked`；
  ② `_LONG_MARCH_TASKLIST.md` 的 G-D52-PERF 行也写 `blocked`；
  ③ `scripts/check_compile_speed.py` 的 git diff 为空（大哥「判据不动」）。

用法: python scripts/check_gd52_row.py
"""
import json
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER = os.path.join(os.path.dirname(REPO), 'grok-build')


def main():
    fails = []
    d = json.load(open(os.path.join(LEDGER, '_TASKLIST.json'), encoding='utf-8'))
    row = next((r for r in d['rows'] if r['id'] == 'G-D52-PERF'), None)
    if not row:
        fails.append('_TASKLIST.json 缺 G-D52-PERF')
    elif row['status'] != 'blocked':
        fails.append('G-D52-PERF status=%s（期望 blocked）' % row['status'])

    lm = os.path.join(LEDGER, '_LONG_MARCH_TASKLIST.md')
    if os.path.exists(lm):
        hit = [l for l in open(lm, encoding='utf-8', errors='replace').read().splitlines()
               if 'G-D52-PERF' in l]
        if not hit or not any(re.search(r'\|\s*blocked\s*\|', l) for l in hit):
            fails.append('_LONG_MARCH_TASKLIST.md 的 G-D52-PERF 行不是 blocked')
    else:
        fails.append('缺 _LONG_MARCH_TASKLIST.md')

    r = subprocess.run(['git', 'diff', '--quiet', '--', 'scripts/check_compile_speed.py'], cwd=REPO)
    if r.returncode != 0:
        fails.append('check_compile_speed.py 有改动（违反「判据不动」）')

    if fails:
        print('[FAIL] G-D52-PERF 台账状态: ' + '; '.join(fails))
        return 1
    print('[PASS] G-D52-PERF: _TASKLIST + _LONG_MARCH 均 blocked；check_compile_speed.py diff 空')
    return 0


if __name__ == '__main__':
    sys.exit(main())
