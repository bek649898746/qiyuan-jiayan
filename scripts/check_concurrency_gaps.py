#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_concurrency_gaps —— ORD-20261104-105 三缺口的现状闸（blocked 的复现判据）。

① 通道发送 move：已落地（转发 check_chan_move.py，应 PASS）。
② 结构化并发两态语法：`失败即停` / `失败各自` 尚未定义 ⟹ 红（等语法裁定）。
③ 编译期数据竞争检查：A3110 尚未登记 ⟹ 红（等口径裁定）。

本脚本**故意在 ②③ 未落地时返回 1**，用于 blocked 条目的 `repro_expect: red`。
用法: python scripts/check_concurrency_gaps.py
"""
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    gaps = []

    r = subprocess.run([sys.executable, os.path.join(ROOT, 'scripts', 'check_chan_move.py')],
                       cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    ok1 = r.returncode == 0
    print('[%s] ① 通道发送 move（4 例）' % ('PASS' if ok1 else 'FAIL'))
    if not ok1:
        gaps.append('①')

    src = io.open(os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy'), 'rb').read().decode('utf-8', 'surrogateescape')
    ok2 = ('失败即停' in src) and ('失败各自' in src)
    print('[%s] ② 结构化并发两态语法（失败即停 / 失败各自）' % ('PASS' if ok2 else 'GAP'))
    if not ok2:
        gaps.append('②')

    ok3 = ('数据竞争' in src)
    print('[%s] ③ 编译期数据竞争检查（A3110）' % ('PASS' if ok3 else 'GAP'))
    if not ok3:
        gaps.append('③')

    if gaps:
        print('=== ORD-105 并发三缺口: 仍有未落地 -> %s ===' % gaps)
        return 1
    print('=== ORD-105 ①②③ 全部落地（③ 含块内 + 线程体两段）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
