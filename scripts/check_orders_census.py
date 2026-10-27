# -*- coding: utf-8 -*-
"""check_orders_census —— 施工递刀单清点闸（2026-10-27 目标计划 step 1）。

判据（**身份级**，不只是数量）：
  ① 编译器仓根目录 `_施工单_*.md` 的文件名集合 == 台账 `ORD-20261027-*` 每行的 `source` 集合；
  ② 无重复 `source`、无漏登、无多登；
  ③ 每条状态 ∈ {fixed, settled, blocked}（无 open / in_progress）；
  ④ 每条 evidence 与 next_step 非空。

用法: python scripts/check_orders_census.py
输出可重定向到 `logs/orders_census.log`。
"""
import glob
import io
import json
import os
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER = os.path.join(os.path.dirname(REPO), 'grok-build')
TL = os.path.join(LEDGER, '_TASKLIST.json')


def main():
    files = sorted(os.path.basename(p) for p in glob.glob(os.path.join(REPO, '_施工单_*.md')))
    d = json.load(io.open(TL, encoding='utf-8'))
    rows = [r for r in d['rows'] if r['id'].startswith('ORD-20261027-')]
    sources = [r.get('source', '') for r in rows]
    fails = []

    fset, sset = set(files), set(sources)
    missing = sorted(fset - sset)
    extra = sorted(sset - fset)
    dup = sorted(x for x in sset if sources.count(x) > 1)
    if missing:
        fails.append('漏登（盘上有单、台账无行）: %s' % ', '.join(missing))
    if extra:
        fails.append('多登（台账有行、盘上无单）: %s' % ', '.join(extra))
    if dup:
        fails.append('重复 source: %s' % ', '.join(dup))
    if '' in sset:
        fails.append('存在 source 为空的 ORD 行: %s' % [r['id'] for r in rows if not r.get('source')])
    if len(files) != len(rows):
        fails.append('数量不一致: 盘上 %d / 台账 %d' % (len(files), len(rows)))

    for r in rows:
        if r['status'] not in ('fixed', 'settled', 'blocked'):
            fails.append('%s 状态=%s（非终态）' % (r['id'], r['status']))
        if not r.get('evidence') or not r.get('next_step'):
            fails.append('%s evidence/next_step 空' % r['id'])

    print('施工单 %d 份 · 台账 ORD-* %d 条 · fixed %d / settled %d / blocked %d' % (
        len(files), len(rows),
        sum(1 for r in rows if r['status'] == 'fixed'),
        sum(1 for r in rows if r['status'] == 'settled'),
        sum(1 for r in rows if r['status'] == 'blocked')))
    if fails:
        print('[FAIL] 施工单清点: %d 项' % len(fails))
        for f in fails:
            print('   - ' + f)
        return 1
    print('[PASS] 施工单清点: %d/%d 身份一一对应、有终态、有证据、有承接入口' % (len(files), len(rows)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
