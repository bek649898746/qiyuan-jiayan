#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_d6_35s —— D6 criterion 4：自编 ≤35 s（连测 3 次）。

当前实测（2026-11-06，编译器 255F50CC，主源 ~40k 行）：墙钟 **56 s**，
profile：词法 1.36 s / 解析 7.46 s / **码生成 34.75 s** / 对象写 2.85 s（合计 46.4 s）。
历史记录 39 s 是 2026-11-02 的 35k 行主源；主源增长 +5k 行后码生成 +4.7 s。

本脚本连测 3 次，全部 ≤35 s 才 PASS；否则 rc=1（blocked 的 `repro_expect: red`）。
用法: python scripts/check_d6_35s.py [--cc qcc_zh_v2.exe]
"""
import argparse
import os
import subprocess
import sys
import tempfile
import time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    ap.add_argument('--limit', type=float, default=35.0)
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    times = []
    with tempfile.TemporaryDirectory(prefix='qcc_d6_') as tmp:
        for i in range(3):
            t0 = time.time()
            r = subprocess.run([cc, 'srclib_jiayan/qcc_zh.jy', '-o', os.path.join(tmp, 'd6_%d.exe' % i)],
                               cwd=ROOT, capture_output=True, timeout=1800)
            dt = time.time() - t0
            times.append(dt)
            ok = (r.returncode == 0)
            print('run%d: %.2fs rc=%d' % (i + 1, dt, r.returncode))
            if not ok:
                print('=== D6: FAIL（编译失败）===')
                return 1
    good = all(t <= a.limit for t in times)
    print('%s 连测 3 次=%s 上限=%.0fs' % ('[PASS]' if good else '[GAP]',
                                       ', '.join('%.2f' % t for t in times), a.limit))
    if good:
        print('=== D6 criterion 4: PASS ===')
        return 0
    print('=== D6 criterion 4: 未达标（差 %.1fs）===' % (min(times) - a.limit))
    return 1


if __name__ == '__main__':
    sys.exit(main())
