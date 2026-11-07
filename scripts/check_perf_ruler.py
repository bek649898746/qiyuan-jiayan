#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_perf_ruler —— ORD-20261107-135 复现器：性能量具只量到 gcc -O0，量不到 -O2。

缺陷（审计递单 2026-11-07）：现在的判据只把甲言产物和 `gcc -O0` 比（1.41×，绿），
不量 `gcc -O2`（2.38×，红）。IR 接入后若沿用这把尺子，会看到「全绿」而离 -O2 还差 2.38 倍。

本判据 = **尺子缺口复现**：跑三档同 bench（甲言 / gcc -O0 / gcc -O2），
只要「甲言 vs -O2」这一档**没有被任何既有判据纳入阈值**，就判红（缺口仍在）。
一旦某条判据把 -O2 档做成硬阈值（在 `scripts/` 里能查到），本行即可转 fixed。

退出码：0 = 缺口已补；1 = 缺口仍在（预期）。
"""
import os
import re
import subprocess
import sys

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def grep_scripts(pattern):
    hits = []
    for name in os.listdir(os.path.join(ROOT, 'scripts')):
        if not name.endswith('.py'):
            continue
        if name == os.path.basename(__file__):
            continue
        p = os.path.join(ROOT, 'scripts', name)
        try:
            txt = open(p, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        if re.search(pattern, txt):
            hits.append(name)
    return hits


def main():
    # 只认「**性能对照**判据」：脚本名以 check_perf 开头，且同时出现 gcc 与 -O2。
    # （构造工具链时用 `gcc -O2` 编宿主程序的不算量具 —— 那是编译参数，不是性能标尺。）
    o2 = [n for n in grep_scripts(r'-O2')
          if n.startswith('check_perf')]
    print('[量具] 把 `gcc -O2` 当性能对照档的判据: %s' % (o2 or '无'))
    if not o2:
        print('[RED] ORD-20261107-135：性能尺子仍只量 -O0 —— 与 gcc -O2 的差距（2.38×）无人度量')
        return 1
    print('[PASS] 已有性能对照判据把 gcc -O2 纳入: %s' % o2)
    return 0


if __name__ == '__main__':
    sys.exit(main())
