#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""两个 `-o` 产物的**指令级归一化 diff**（`-o` 线的定位尺）。

为什么不能直接比字节：`-o` 产物里所有 RIP 相对位移都随布局漂移
（实测首要差异永远是 `f2 0f 10 05 <disp32>` 的 2 字节位移差），
直接 `cmp` 只会得到「处处不同」。

本尺子做法：
  1. `objdump -d --no-show-raw-insn` 取两边指令；
  2. 归一化：去 `#` 注释、去 `rex`/`rex.X` 前缀、把 `-0x…(%rip)` 归成 `RIP(%rip)`、
     跳转/调用目标归成 `TGT`；
  3. 顺序比对，遇差异在 ±WINDOW 内找最近重同步点，输出前 N 个 hunk。

判据（AC3/AC1 用）：**同一构建的两个产物**之间跑（不跨构建 —— 节点号/cp/跨度边界都会漂移）。
输出里第一处结构性差异就是「下一刀该修的那一族」。

用法：
    python scripts\\ir_xdiff.py A.exe B.exe            # 默认 6 个 hunk、窗口 8
    python scripts\\ir_xdiff.py A.exe B.exe --hunks 12 --window 12
"""
import argparse
import re
import subprocess
import sys


def insns(path):
    out = subprocess.run(['objdump', '-d', '--no-show-raw-insn', path],
                         capture_output=True).stdout.decode('utf-8', 'replace')
    res = []
    for line in out.splitlines():
        m = re.match(r'^\s+[0-9a-f]+:\t(.*)$', line)
        if not m:
            continue
        t = m.group(1).split('#')[0].strip()
        t = re.sub(r'^rex(\.\w+)? ', '', t)
        t = re.sub(r'-?0x[0-9a-f]+\(%rip\)', 'RIP(%rip)', t)
        t = re.sub(r'\s+', ' ', t)
        if re.match(r'^(j\w+|call)\s', t):
            t = t.split()[0] + ' TGT'
        res.append(t)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('a')
    ap.add_argument('b')
    ap.add_argument('--hunks', type=int, default=6)
    ap.add_argument('--window', type=int, default=8)
    a = ap.parse_args()

    A, B = insns(a.a), insns(a.b)
    print('A insns %d | B insns %d  (delta %+d)' % (len(A), len(B), len(B) - len(A)))
    i = j = shown = 0
    while i < len(A) and j < len(B) and shown < a.hunks:
        if A[i] == B[j]:
            i += 1
            j += 1
            continue
        best = None
        for di in range(0, a.window + 1):
            for dj in range(0, a.window + 1):
                if di == 0 and dj == 0:
                    continue
                if i + di < len(A) and j + dj < len(B) and A[i + di] == B[j + dj]:
                    if best is None or di + dj < best[0]:
                        best = (di + dj, di, dj)
        if best is None:
            print('no resync at A[%d] B[%d]' % (i, j))
            break
        di, dj = best[1], best[2]
        print('=== hunk A[%d:%d] B[%d:%d] ===' % (i, i + di, j, j + dj))
        for k in range(i, i + di):
            print('   A-', A[k])
        for k in range(j, j + dj):
            print('   B-', B[k])
        shown += 1
        i += di
        j += dj
    return 0


if __name__ == '__main__':
    sys.exit(main())
