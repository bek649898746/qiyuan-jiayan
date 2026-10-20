# -*- coding: utf-8 -*-
"""check_anchor_err_channel — 审计 P1-2 判据（2026-10-21 虾米）

病灶：8 个锚把「stderr 上的运行时错误文本」写成 `@EXPECTED out:` ⟹ 行为是对的、断言写反了 ⟹
全量恒 FAIL=8。`run_tests_zh.py` 本来就支持 `@EXPECTED err:`。

判据：用**出厂的闸**（`run_tests_zh.py --only --parallel 1`）逐个跑这 8 个锚，全部 PASS=1。
（把任一锚改回 `out:` 会立刻 FAIL ⟹ 能证伪回退。）

用法: python scripts/check_anchor_err_channel.py
"""
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)

ANCHORS = [
    'feat_zh_refcell_conflict.c',
    'feat_zh_bound_divzero.c',
    'feat_zh_cell_underflow_rt.c',
    'feat_zh_cell_conflict_rt.c',
    'feat_zh_leak86.c',
    'feat_zh_leak86_leak.c',
    'feat_zh_div_zero_compound.c',
    'feat_zh_ws61_off_neg.c',
]


def main():
    bad = []
    for n in ANCHORS:
        r = subprocess.run([sys.executable, 'scripts/run_tests_zh.py', '--only',
                            'scripts/' + n, '--parallel', '1'],
                           capture_output=True, text=True, encoding='utf-8', errors='replace',
                           timeout=600)
        ok = (r.returncode == 0 and 'PASS=1' in ((r.stdout or '') + (r.stderr or '')).replace(' ', ''))
        print('  %-34s %s' % (n, 'PASS' if ok else 'FAIL'))
        if not ok:
            bad.append(n)
    print('=== AUDIT-P1-2 判据（8 锚 err: 通道）: %s (%d/8) ==='
          % ('PASS' if not bad else 'FAIL', len(ANCHORS) - len(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
