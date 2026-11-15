#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ir_dis_diff.py A.o B.o <函数名> [--ctx N] [--norm]

两模式 .o 的函数级反汇编差异，地址一律换成**函数内相对偏移**（+0x..），
避免「前面某个函数少 4 字节」把整段地址整体平移、误导 difflib 对齐。

--norm（推荐）：把跳转/调用的**目标地址**归一化
    · 函数内跳转（jcc/jmp）⟹ `L`（局部目标，大小变化属连带效应，不是结构差）
    · 调用/外部跳转 ⟹ 只保留符号名（去掉随文件布局漂移的绝对地址）
    归一化后剩下的差异才是**真结构差异**（少了/多了指令）。

用法示例：
    python scripts\\ir_dis_diff.py build\\a.o build\\b.o luaF_initupvals --ctx 3 --norm
"""
import subprocess, sys, re, difflib

JUMP = re.compile(r'^(j\w+|call|loop\w*)\s+(?:[0-9a-f]+)\s+<([^>]+)>')


def norm(text, fname):
    text = re.sub(r'\s+#.*$', '', text)          # 去掉 objdump 的地址注释
    m = JUMP.match(text)
    if not m:
        return text
    op, sym = m.group(1), m.group(2)
    if sym.startswith(fname + '+'):      # 函数内目标
        return op + ' L'
    if sym == fname:
        return op + ' L'
    return op + ' <' + sym.split('+')[0] + '>'   # 外部目标只留符号名


def dis(obj, fn):
    p = subprocess.run(['objdump', '-d', '--no-show-raw-insn',
                        '--disassemble=' + fn, obj],
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    out = []
    base = None
    for l in p.stdout.split('\n'):
        m = re.match(r'\s+([0-9a-f]+):\t(.*)$', l)
        if m:
            a = int(m.group(1), 16)
            if base is None:
                base = a
            out.append((a - base, m.group(2).strip()))
    return out, base


def main():
    a_o, b_o, fn = sys.argv[1], sys.argv[2], sys.argv[3]
    ctx = 3
    if '--ctx' in sys.argv:
        ctx = int(sys.argv[sys.argv.index('--ctx') + 1])
    do_norm = '--norm' in sys.argv
    A, abase = dis(a_o, fn)
    B, bbase = dis(b_o, fn)
    sa = [norm(x[1], fn) if do_norm else x[1] for x in A]
    sb = [norm(x[1], fn) if do_norm else x[1] for x in B]
    print('== %s ==  A=%d B=%d instr  (A@0x%x B@0x%x, norm=%s)' %
          (fn, len(A), len(B), abase, bbase, do_norm))
    sm = difflib.SequenceMatcher(None, sa, sb, autojunk=False)
    n = 0
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            continue
        n += 1
        print('--- #%d %s  A[%d:%d]  B[%d:%d]' % (n, tag, i1, i2, j1, j2))
        for k in range(max(0, i1 - ctx), i1):
            print('   +0x%04x  %s' % (A[k][0], A[k][1]))
        for k in range(i1, i2):
            print(' A +0x%04x  %s' % (A[k][0], A[k][1]))
        for k in range(j1, j2):
            print(' B +0x%04x  %s' % (B[k][0], B[k][1]))
        for k in range(j2, min(len(B), j2 + ctx)):
            print('   +0x%04x  %s' % (B[k][0], B[k][1]))
    print('== 差异块 %d 个，指令数差 A-B = %d ==' % (n, len(A) - len(B)))


if __name__ == '__main__':
    main()
