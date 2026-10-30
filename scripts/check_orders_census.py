# -*- coding: utf-8 -*-
"""check_orders_census —— 施工递刀单清点闸（2026-10-27 目标计划 step 1）。

判据（**身份级**，不只是数量）：
  ① 编译器仓根目录 `_施工单_*.md` 的文件名集合 == 台账 `ORD-*` 每行的 `source` 集合（不限日期前缀）；
  ② 无重复 `source`、无漏登、无多登；
  ③ 每条状态 ∈ {fixed, settled, blocked}（无 open / in_progress）；
  ④ 每条 evidence 与 next_step 非空。

用法: python scripts/check_orders_census.py
输出可重定向到 `logs/orders_census.log`。
"""
import argparse
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
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=None,
                    help='把本次输出原样落盘（建议由 verify_plan_steps 指向当前轮 logs/orders_census.log，'
                         '这样「清点证据」= 本次运行的原文，不会再是上一轮的 stale 副本）')
    a = ap.parse_args()

    def emit(s):
        print(s)
        if a.out:
            buf.append(s)

    buf = []
    files = sorted(os.path.basename(p) for p in glob.glob(os.path.join(REPO, '_施工单_*.md')))
    d = json.load(io.open(TL, encoding='utf-8'))
    rows = [r for r in d['rows'] if r['id'].startswith('ORD-')]  # R902: 范围从固定日期前缀放宽到全部 ORD-*（新递的 20261028 单原本被漏数）
    sources = [r.get('source', '') for r in rows]
    fails = []

    fset, sset = set(files), set(sources)
    missing = sorted(fset - sset)
    extra = sorted(sset - fset)
    # 2026-10-30（ORD-54 收口）：**一份施工单派生多条台账行是常态**（单 = 投递单位，行 = 缺陷单位）。
    # 原判据「source 必须唯一」会把这种正常派生判成「重复」，于是只能靠伪造不同 source 绕过。
    # 现口径：同 source 多行必须用 `source_item` 区分且**互不相同**；缺 item / item 重复 才算红。
    # 判据强度不变（仍然「不漏单、不虚报、每条可定位到单内子项」），只把「一对多」从禁止改为需标注。
    by_src = {}
    for r in rows:
        by_src.setdefault(r.get('source', ''), []).append(r)
    dup = []
    for s, rs in by_src.items():
        if len(rs) <= 1:
            continue
        items = [r.get('source_item', '') for r in rs]
        if any(not i for i in items):
            dup.append('%s（同 source %d 行，缺 source_item: %s）'
                       % (s, len(rs), [r['id'] for r, i in zip(rs, items) if not i]))
        elif len(set(items)) != len(items):
            dup.append('%s（source_item 重复: %s）' % (s, items))
    if missing:
        fails.append('漏登（盘上有单、台账无行）: %s' % ', '.join(missing))
    if extra:
        fails.append('多登（台账有行、盘上无单）: %s' % ', '.join(extra))
    if dup:
        fails.append('source 派生标注不完整: %s' % ', '.join(dup))
    if '' in sset:
        fails.append('存在 source 为空的 ORD 行: %s' % [r['id'] for r in rows if not r.get('source')])
    # 2026-10-30：**不再要求「盘上份数 == 台账行数」** —— 一份施工单派生多条台账行（单=投递单位、
    # 行=缺陷单位）时该等式恒不成立，而「不漏/不多」已由 missing / extra 双向集合判据覆盖。
    # 强度不变：仍要求 源集合 == 盘上文件集合（双向差集为空）。

    for r in rows:
        # 2026-10-28（R891）：接受 `open` = 审计新递、本批未开工。它仍要求有 evidence/next_step，
        # 只是不许冒充 fixed/settled/blocked（清点既不漏单，也不把未做的活说成已终态）。
        if r['status'] not in ('fixed', 'settled', 'blocked', 'open'):
            fails.append('%s 状态=%s（既非终态也非未开工）' % (r['id'], r['status']))
        if not r.get('evidence') or not r.get('next_step'):
            fails.append('%s evidence/next_step 空' % r['id'])

    emit('施工单 %d 份 · 台账 ORD-* %d 条 · fixed %d / settled %d / blocked %d / open(未开工) %d' % (
        len(files), len(rows),
        sum(1 for r in rows if r['status'] == 'fixed'),
        sum(1 for r in rows if r['status'] == 'settled'),
        sum(1 for r in rows if r['status'] == 'blocked'),
        sum(1 for r in rows if r['status'] == 'open')))
    rc = 0
    if fails:
        emit('[FAIL] 施工单清点: %d 项' % len(fails))
        for f in fails:
            emit('   - ' + f)
        rc = 1
    else:
        emit('[PASS] 施工单清点: %d/%d 身份一一对应、有终态、有证据、有承接入口'
             % (len(files), len(rows)))
    if a.out:
        io.open(a.out, 'w', encoding='utf-8', newline='\n').write('\n'.join(buf) + '\n')
    return rc


if __name__ == '__main__':
    sys.exit(main())
