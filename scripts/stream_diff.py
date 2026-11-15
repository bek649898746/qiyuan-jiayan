# -*- coding: utf-8 -*-
"""以 **legacy 的指令边界** 为锚，逐条比较两模式 .text 的字节。

为什么这样：IR 侧存在若干「整段拷贝」通道（`内建回放` / J1 录操 / 增减捕获），
它们不经过 `汇发`，日志里没有逐条指令。但只要以 legacy 边界为锚直接比字节，
这些通道自动被覆盖（拷贝正确则不报，错误则报到具体 legacy 指令上）。

尾部署性差异（同一 legacy 指令 + 同长度 + 仅尾部 rel32/disp32 不同）视为布局后果，跳过。

用法:
  python scripts/stream_diff.py <leg.log> <ir.log> <leg.o> <ir.o> [--max N] [--from A] [--to B]
                                 [--fn 名字 --obj xx.o]
"""
import io
import os
import re
import subprocess
import sys

OBJCOPY = r'C:\mingw-w64\bin\objcopy.exe'
OBJDUMP = r'C:\mingw-w64\bin\objdump.exe'
SITE = re.compile(r'^\[SITE\] cp=(\d+) sm=(\d+) cap=(\d+) ?(.*)$')


def text_bytes(obj):
    out = obj + '.text.bin'
    subprocess.run([OBJCOPY, '--only-section=.text', '-O', 'binary', obj, out], check=True)
    b = io.open(out, 'rb').read()
    os.remove(out)
    return b


def load(log):
    rows = []
    for line in io.open(log, encoding='utf-8', errors='replace'):
        m = SITE.match(line.rstrip('\n'))
        if not m:
            continue
        cp, sm, cap, txt = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4)
        if sm or cap or not txt or txt.startswith(';') or txt.startswith('.L'):
            continue
        rows.append((cp, txt))
    rows.sort()
    return rows


def func_range(obj, name):
    r = subprocess.run([OBJDUMP, '-t', obj], capture_output=True)
    syms = []
    for line in r.stdout.decode('utf-8', 'replace').splitlines():
        m = re.match(r'^\[\s*\d+\]\(sec\s+(\d+)\).*?0x([0-9a-f]+) (\S+)$', line.strip())
        if m and m.group(1) == '1':
            syms.append((int(m.group(2), 16), m.group(3)))
    syms.sort()
    for i, (off, nm) in enumerate(syms):
        if nm == name:
            return off, (syms[i + 1][0] if i + 1 < len(syms) else 1 << 30)
    return None


def same(a, b):
    """同一 legacy 指令的字节等价（允许尾部 rel32/disp32 布局差异）。"""
    if a == b:
        return True
    if len(a) == len(b) and len(a) >= 5 and a[:-4] == b[:-4]:
        return True
    if len(a) == len(b) and len(a) >= 2 and a[:-1] == b[:-1]:
        return True
    return False


def main():
    a_log, b_log, a_obj, b_obj = sys.argv[1:5]
    mx = 8 if '--max' not in sys.argv else int(sys.argv[sys.argv.index('--max') + 1])
    lo, hi = 0, 1 << 40
    if '--from' in sys.argv:
        lo = int(sys.argv[sys.argv.index('--from') + 1])
    if '--to' in sys.argv:
        hi = int(sys.argv[sys.argv.index('--to') + 1])
    if '--fn' in sys.argv:
        rg = func_range(sys.argv[sys.argv.index('--obj') + 1], sys.argv[sys.argv.index('--fn') + 1])
        if not rg:
            print('找不到函数')
            return
        lo, hi = rg
        print('函数 %s 区间 [%d, %d)' % (sys.argv[sys.argv.index('--fn') + 1], lo, hi))
    A = load(a_log)
    ta, tb = text_bytes(a_obj), text_bytes(b_obj)
    n = shown = 0
    for i, (cp, txt) in enumerate(A):
        if not (lo <= cp < hi):
            continue
        nxt = A[i + 1][0] if i + 1 < len(A) else len(ta)
        aa = ta[cp:nxt]
        bb = tb[cp:nxt]
        if same(aa, bb):
            continue
        n += 1
        if shown < mx:
            shown += 1
            print('--- 差异 #%d @cp=%d  %s' % (n, cp, txt))
            for k in range(max(lo, i - 2), i):
                print('    前 %6d | %s' % (A[k][0], A[k][1]))
            print('  >> L [%s]' % aa.hex(' '))
            print('  >> I [%s]' % bb.hex(' '))
    print('合计差异 = %d（legacy 指令 %d 条；.text L=%d I=%d）' % (n, len(A), len(ta), len(tb)))


main()
