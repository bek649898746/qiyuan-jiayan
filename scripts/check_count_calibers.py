# -*- coding: utf-8 -*-
"""check_count_calibers — D-COUNT 的**当前可复跑**口径互检（2026-10-21 虾米）

背景：台账原登记的 `scripts/check_count_calibers.py` **并不存在**（与 AI-D17 同类的
「登记名缺失」）。原判据是历史一次性的「三套口径互检」（291=291、366=116+250、净缺口=…），
在当前树上无法逐字复现。本脚本把「口径互检」固化成两条**会真失败**的一致性断言：

  ① 台账内部一致：`_BACKLOG_QUEUE.md` 的 **行数 == 各状态计数之和**
  ② 闸内引用一致：`run_tests_zh.py` 的 `CORE_FEAT` 里每个锚 **文件都存在**

用法: python scripts/check_count_calibers.py [--ledger <path>]
退出码: 0 PASS / 1 FAIL
"""
import argparse
import io
import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ledger', default=os.path.join(os.path.dirname(REPO), 'grok-build',
                                                     '_BACKLOG_QUEUE.md'))
    a = ap.parse_args()
    ok = True

    # ① 台账内部一致
    if os.path.exists(a.ledger):
        s = io.open(a.ledger, encoding='utf-8').read()
        rows = [l for l in s.split('\n') if l.startswith('|') and not l.startswith('|:')
                and not l.startswith('| ID')]
        cnt = {}
        for l in rows:
            cells = [x.strip() for x in l.strip('|').split('|')]
            for st in ('fixed', 'settled', 'blocked', 'open', 'in_progress'):
                if st in cells:
                    cnt[st] = cnt.get(st, 0) + 1
                    break
        tot = sum(cnt.values())
        good = (tot == len(rows))
        print('① 台账: rows=%d  状态和=%d  %s' % (len(rows), tot, cnt))
        print('   -> %s' % ('PASS' if good else 'FAIL'))
        ok &= good
    else:
        print('① 台账: 未找到 %s（跳过，不算失败）' % a.ledger)

    # ② CORE_FEAT 引用一致
    src = io.open(os.path.join('scripts', 'run_tests_zh.py'), encoding='utf-8', errors='replace').read()
    i = src.find('CORE_FEAT = [')
    j = src.find(']', i)
    block = src[i:j] if i >= 0 else ''
    anchors = re.findall(r"'(scripts/[^']+\.c)'", block)
    missing = [x for x in anchors if not os.path.exists(x)]
    print('② CORE_FEAT: %d 条；缺失文件 %d 个 %s' % (len(anchors), len(missing), missing[:5]))
    good2 = (not missing and len(anchors) > 0)
    print('   -> %s' % ('PASS' if good2 else 'FAIL'))
    ok &= good2

    print('=== D-COUNT 口径互检: %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
