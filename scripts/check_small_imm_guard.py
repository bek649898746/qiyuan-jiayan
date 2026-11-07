#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_small_imm_guard —— ORD-20261107-136 复现器：`小立即` pass 在热路径上反而慢。

缺陷（审计递单 2026-11-07）：`srclib/opt_zh.jy` 的 `小立即()` 把 `移动 rX, v`（v ≤ 127）
换成 `压栈 v` + `弹栈 rX` —— 省 4 字节，但两次内存往返。实测热循环慢 16.3–17.2%。

本判据：同一 bench 编两次（默认 / `--no-16` 关掉该 pass），比墙钟。
慢超过阈值就判红（缺陷仍在）；也接受「该 pass 已加守卫」的情形 ——
此时默认与 --no-16 的差距会落到阈值内，判据转绿。

退出码：0 = 已不慢（或 pass 已不可关）；1 = 仍在慢（预期）。
"""
import io
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = os.path.join(ROOT, 'qcc_zh_v1.exe')

SRC = r'''#include <stdio.h>
int main(void) {
    volatile long acc = 0;
    for (long r = 0; r < 20; r++)
        for (long i = 0; i < 1000000; i++) acc += (i & 7);
    printf("%ld\n", acc);
    return 0;
}
'''

LIMIT_PCT = 5.0      # 超过 5% 就认为「热路径守卫」还没做
RUNS = 3


def build(work, extra, tag):
    src = os.path.join(work, 'b_%s.c' % tag)
    exe = os.path.join(work, 'b_%s.exe' % tag)
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
    cmd = [CC, src, '-o', exe] + extra
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True)
    return exe if r.returncode == 0 and os.path.exists(exe) else None


def best_of(exe):
    ts = []
    for _ in range(RUNS):
        t0 = time.time()
        subprocess.run([exe], capture_output=True)
        ts.append((time.time() - t0) * 1000.0)
    return min(ts)


def main():
    work = tmp_dir('smallimm')
    try:
        a = build(work, [], 'on')
        b = build(work, ['--no-16'], 'off')
        if not a:
            print('[FAIL] 默认档编译失败')
            return 1
        if not b:
            # 不能因为「关不掉」就报绿 —— 那正是 fail-open。
            print('[RED] `--no-16` 档编译失败：无法把该 pass 关掉来对照 ⟹ 缺陷是否仍在**无法判定**，'
                  '按 fail-closed 判红（ORD-20261107-136）')
            return 1
        ta, tb = best_of(a), best_of(b)
        pct = (ta - tb) / tb * 100.0 if tb > 0 else 0.0
        print('[量] 默认 %.1f ms · --no-16 %.1f ms ⟹ 默认慢 %.1f%%（阈值 %.1f%%）'
              % (ta, tb, pct, LIMIT_PCT))
        if pct > LIMIT_PCT:
            print('[RED] ORD-20261107-136：`小立即` 仍在热路径上造成 %.1f%% 损失' % pct)
            return 1
        print('[PASS] 热路径损失已在阈值内（%.1f%%）' % pct)
        return 0
    finally:
        import shutil
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
