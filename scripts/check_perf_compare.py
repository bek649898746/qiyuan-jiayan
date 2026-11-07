#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_perf_compare —— ORD-20261107-135 判据：`gcc -O2` 作为**对照档**纳入性能闸。

口径（大哥 2026-11-07 定档）：**不许用取舍框架** —— 只量 `-O0` 会看到「全绿」，
而离 `-O2` 还差一大截。本判据同时给出三档与两个比值，并在任一档退化时判红。

判据（同一 bench，各连跑 3 次取最快）：
  · 甲言 / gcc -O0 ≤ 1.75×   （2026-11-07 基线 1.61×，留 9% 回归余量）
  · 甲言 / gcc -O2 ≤ 2.50×   （2026-11-07 基线 2.23×，留 12% 回归余量）
  · 缺 gcc ⟹ fail-closed（不许空过）

用法: python scripts/check_perf_compare.py [--cc qcc_zh_v1.exe] [--json]
退出码: 0 = 两档都在余量内；1 = 任一档越线或缺 gcc。
"""
import argparse
import io
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

LIMIT_O0 = 1.75
LIMIT_O2 = 2.50

C_BENCH = r'''#include <stdio.h>
int main(void) {
    volatile long acc = 0;
    for (long r = 0; r < 20; r++)
        for (long i = 0; i < 1000000; i++) acc += (i & 7);
    printf("%ld\n", acc);
    return 0;
}
'''

JY_BENCH = r'''整 主(空) {
    整 长 acc = 0;
    遍 (整 长 r = 0; r < 20; r++)
        遍 (整 长 i = 0; i < 1000000; i++) acc = acc + (i & 7);
    格式化打印("%ld\n", acc);
    返 0;
}
'''


def best_of(exe, runs=3):
    ts = []
    for _ in range(runs):
        t0 = time.time()
        subprocess.run([exe], capture_output=True)
        ts.append(time.time() - t0)
    return min(ts) * 1000.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--limit-o0', type=float, default=LIMIT_O0, help='负例自检用：把阈值压低 ⟹ 必须判红')
    ap.add_argument('--limit-o2', type=float, default=LIMIT_O2)
    a = ap.parse_args()
    lim_o0, lim_o2 = a.limit_o0, a.limit_o2
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)

    gcc = None
    for cand in ('gcc', 'x86_64-w64-mingw32-gcc'):
        if subprocess.run([cand, '--version'], capture_output=True).returncode == 0:
            gcc = cand
            break
    if not gcc:
        print('[FAIL] 缺 gcc ⟹ 无法建立 `-O0`/`-O2` 对照档（fail-closed，不空过）')
        return 1

    work = tmp_dir('perfcmp')
    try:
        csrc = os.path.join(work, 'b.c')
        io.open(csrc, 'w', encoding='utf-8', newline='\n').write(C_BENCH)
        g0 = os.path.join(work, 'g0.exe')
        g2 = os.path.join(work, 'g2.exe')
        if subprocess.run([gcc, '-O0', csrc, '-o', g0], capture_output=True).returncode != 0:
            print('[FAIL] gcc -O0 构建失败')
            return 1
        if subprocess.run([gcc, '-O2', csrc, '-o', g2], capture_output=True).returncode != 0:
            print('[FAIL] gcc -O2 构建失败')
            return 1

        jsrc = os.path.join(work, 'b.jy')
        io.open(jsrc, 'w', encoding='utf-8', newline='\n').write(JY_BENCH)
        jy = os.path.join(work, 'jy.exe')
        r = subprocess.run([cc, jsrc, '-o', jy], cwd=ROOT, capture_output=True)
        if r.returncode != 0 or not os.path.exists(jy):
            print('[FAIL] 甲言 bench 编译失败 rc=%d %s'
                  % (r.returncode, (r.stdout + r.stderr).decode('utf-8', 'replace')[:160]))
            return 1

        jy_ms, o0_ms, o2_ms = best_of(jy), best_of(g0), best_of(g2)
        r0, r2 = jy_ms / o0_ms, jy_ms / o2_ms
        print('[量] 甲言 %.1f ms · gcc -O0 %.1f ms · gcc -O2 %.1f ms' % (jy_ms, o0_ms, o2_ms))
        print('[比] 甲言/-O0 = %.2f×（阈值 %.2f×）· 甲言/-O2 = %.2f×（阈值 %.2f×）'
              % (r0, lim_o0, r2, lim_o2))
        if a.json:
            print('{"jy_ms": %.3f, "o0_ms": %.3f, "o2_ms": %.3f, "r_o0": %.3f, "r_o2": %.3f}'
                  % (jy_ms, o0_ms, o2_ms, r0, r2))
        bad = []
        if r0 > lim_o0:
            bad.append('对 -O0 %.2f× > %.2f×' % (r0, lim_o0))
        if r2 > lim_o2:
            bad.append('对 -O2 %.2f× > %.2f×' % (r2, lim_o2))
        if bad:
            print('[RED] ORD-20261107-135 性能退化：%s' % '；'.join(bad))
            return 1
        print('=== ORD-20261107-135 判据: PASS（-O0 与 -O2 两档都在余量内）===')
        return 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
