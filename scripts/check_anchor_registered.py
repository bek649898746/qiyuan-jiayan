#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""锚入闸一致性检查（V5 · 批次 F-6）—— 防「新锚忘了入闸」

背景（2026-09-28 虾米 · 审计）:
  · 挂账 V5 = 「各站新锚**不自动入闸**」⟹ 靠人手记得加 `CORE_FEAT` 一行。
  · 同族教训 E6: 闸的**正则漏检**（>6 空格缩进）⟹ "闸够不着"就是盲区 —— 本脚本把
    「文件存在但没登记」这一类盲区也堵上（与 E6 同属「闸完整性」族）。

规则:
  `scripts/` 下所有 `feat_*.c`（含 `_neg` 负面锚）都应出现在 `run_tests_zh.py` 的 `CORE_FEAT` 里；
  例外（刻意不入闸，只作留档/诊断）: 以 `diag_` / `probe_` / `_` 开头的文件。

用法（在仓库根或任意处）:
  python memory/atlas/tools/check_anchor_registered.py      # 或
  python scripts/check_anchor_registered.py
退出码: 0 = 全部入闸; 1 = 有漏（列出）。
"""
import os
import re
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

HERE = os.path.dirname(os.path.abspath(__file__))
# 兼容放在 scripts/ 或 memory/atlas/tools/ 两种位置
CAND = [HERE, os.path.dirname(HERE), os.path.join(os.path.dirname(HERE), 'scripts')]
SCRIPTS = None
for c in CAND:
    if os.path.isfile(os.path.join(c, 'run_tests_zh.py')):
        SCRIPTS = c
        break
if SCRIPTS is None:
    # 再向上找两级
    up = os.path.abspath(os.path.join(HERE, '..', '..'))
    for c in (os.path.join(up, 'scripts'), up):
        if os.path.isfile(os.path.join(c, 'run_tests_zh.py')):
            SCRIPTS = c
            break
if SCRIPTS is None:
    print('[ERR] 找不到 scripts/run_tests_zh.py —— 请从仓库内运行')
    sys.exit(2)

RUNNER = os.path.join(SCRIPTS, 'run_tests_zh.py')
EXCLUDE_PREFIX = ('diag_', 'probe_', '_')

src = open(RUNNER, encoding='utf-8', errors='replace').read()
registered = set(re.findall(r"'(scripts/[\w\u4e00-\u9fff\-\.]+\.c)'", src))

missing, total = [], 0
for fn in sorted(os.listdir(SCRIPTS)):
    if not (fn.endswith('.c') and fn.startswith('feat_')):
        continue
    if fn.startswith(EXCLUDE_PREFIX):
        continue
    total += 1
    if ('scripts/' + fn) not in registered:
        missing.append('scripts/' + fn)

print('[锚入闸检查] run_tests_zh.py 的 CORE_FEAT 已登记 = %d 条' % len(registered))
print('[锚入闸检查] scripts/feat_*.c 实有 = %d 个' % total)
if missing:
    print('[!!] 未入闸 %d 个（应补 CORE_FEAT）:' % len(missing))
    for m in missing:
        print('   - ' + m)
    sys.exit(1)
print('[OK] scripts/feat_*.c 全部已入闸')
