# -*- coding: utf-8 -*-
"""定位两模式 .text 的**第一个字节差异**，并给出两侧的 SITE 上下文。

用法:
  python scripts/first_byte_diff.py build/xx.leg.o build/xx.ir.o [--ctx 5]

步骤：
  1. objcopy 抽出 .text → 原始二进制
  2. 逐字节比较，取第一个差异偏移
  3. 打印两侧该偏移附近的字节（hex）与最近的 SITE 指令行
"""
import io
import os
import re
import subprocess
import sys

OBJCOPY = r'C:\mingw-w64\bin\objcopy.exe'
LINE = re.compile(r'^\[SITE\] cp=(\d+) sm=(\d+) cap=(\d+) ?(.*)$')


def text_bytes(obj, out):
    subprocess.run([OBJCOPY, '--only-section=.text', '-O', 'binary', obj, out], check=True)
    return io.open(out, 'rb').read()


def site_rows(log):
    rows = []
    for line in io.open(log, encoding='utf-8', errors='replace'):
        m = LINE.match(line.rstrip('\n'))
        if m:
            rows.append((int(m.group(1)), m.group(4)))
    return rows


def near(rows, off, k=4):
    idx = 0
    for i, (cp, _) in enumerate(rows):
        if cp <= off:
            idx = i
        else:
            break
    return rows[max(0, idx - k + 1): idx + 1]


def main():
    a_obj, b_obj = sys.argv[1], sys.argv[2]
    ctx = 4 if '--ctx' not in sys.argv else int(sys.argv[sys.argv.index('--ctx') + 1])
    a = text_bytes(a_obj, 'build/_a.text')
    b = text_bytes(b_obj, 'build/_b.text')
    print('.text 长度 L=%d I=%d' % (len(a), len(b)))
    n = min(len(a), len(b))
    off = None
    for i in range(n):
        if a[i] != b[i]:
            off = i
            break
    if off is None:
        print('前缀完全一致（差异只在长度/尾部）')
        off = n
    print('第一个差异偏移 = %d' % off)
    lo = max(0, off - 24)
    print('L[%d:%d] = %s' % (lo, off + 32, a[lo:off + 32].hex(' ')))
    print('I[%d:%d] = %s' % (lo, off + 32, b[lo:off + 32].hex(' ')))
    for tag, path in (('L', sys.argv[3] if len(sys.argv) > 3 else None), ('I', sys.argv[4] if len(sys.argv) > 4 else None)):
        if not path:
            continue
        print('--- %s 侧最近 SITE ---' % tag)
        for cp, txt in near(site_rows(path), off, ctx):
            print('   %6d | %s' % (cp, txt))


main()
