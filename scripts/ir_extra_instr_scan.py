#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ir_extra_instr_scan.py —— 全语料「legacy 有、IR 无」的指令清单（按形状归类）

为什么需要
    `ir_instr_diff.py` 只给总数；`ir_dis_diff.py` 只给单函数。而真根治要的是
    「**哪一族缺什么形状的指令、各出现几次**」。本脚本把 34 个 lua 文件两模式各编一次，
    逐函数做序列比对（跳转/调用目标归一化），把 `delete`（legacy 独有）与
    `insert`（IR 独有）的指令按 `助记符 + 操作数骨架` 归并计数。

用法
    python scripts\\ir_extra_instr_scan.py                # 全部 34 文件
    python scripts\\ir_extra_instr_scan.py lvm lfunc      # 只扫指定文件
"""
import os, re, sys, subprocess, difflib, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORPUS = r'D:\qcc_corpus\lua-5.4.7\src'
CC = os.path.join(ROOT, 'qcc_zh_v2.exe')
JUMP = re.compile(r'^(j\w+|call|loop\w*)\s+(?:[0-9a-f]+)\s+<([^>]+)>')


def norm(text, fname):
    text = re.sub(r'\s+#.*$', '', text)
    m = JUMP.match(text)
    if not m:
        return re.sub(r'-?0x[0-9a-f]+', 'K', text)      # 位移/常量归一
    op, sym = m.group(1), m.group(2).split('+')[0]
    if sym == fname:
        return op + ' L'
    return op + ' <' + sym + '>'


def dis(obj):
    p = subprocess.run(['objdump', '-d', '--no-show-raw-insn', obj],
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    funcs, cur = collections.OrderedDict(), None
    for l in p.stdout.split('\n'):
        m = re.match(r'^[0-9a-f]+ <([^>]+)>:$', l.strip())
        if m:
            cur = m.group(1); funcs[cur] = []; continue
        m = re.match(r'\s+([0-9a-f]+):\t(.*)$', l)
        if m and cur:
            funcs[cur].append(m.group(2).strip())
    return funcs


def compile_one(src, mode, out):
    env = dict(os.environ)
    env['QCC_CODEGEN'] = mode
    rt = os.path.join(ROOT, 'srclib', 'qcc_rt.c')
    if os.path.exists(rt):
        env['QCC_RT_PATH'] = rt
    p = subprocess.run([CC, src, '-c', '-o', out], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', env=env)
    return p.returncode


def shape(text):
    t = text.split()[0] if text.split() else '?'
    sb = re.sub(r'%[a-z0-9]+', '%r', text)
    sb = re.sub(r'\$?K', 'K', sb)
    return sb


def main():
    only = set(sys.argv[1:])
    files = sorted(f[:-2] for f in os.listdir(CORPUS)
                   if f.endswith('.c') and (not only or f[:-2] in only))
    tmp = os.path.join(ROOT, 'build', '_scan')
    os.makedirs(tmp, exist_ok=True)
    del_tab = collections.Counter()
    ins_tab = collections.Counter()
    per_file = collections.Counter()
    for name in files:
        src = os.path.join(CORPUS, name + '.c')
        a, b = os.path.join(tmp, name + '_leg.o'), os.path.join(tmp, name + '_ir.o')
        if compile_one(src, 'legacy', a) or compile_one(src, 'ir', b):
            print('SKIP', name, '(compile rc!=0)'); continue
        fa, fb = dis(a), dis(b)
        for fn in fa:
            if fn not in fb:
                continue
            sa = [norm(x, fn) for x in fa[fn]]
            sb = [norm(x, fn) for x in fb[fn]]
            sm = difflib.SequenceMatcher(None, sa, sb, autojunk=False)
            for tag, i1, i2, j1, j2 in sm.get_opcodes():
                if tag in ('delete', 'replace'):
                    for k in range(i1, i2):
                        del_tab[shape(fa[fn][k])] += 1
                        per_file[name] += 1
                if tag in ('insert', 'replace'):
                    for k in range(j1, j2):
                        ins_tab[shape(fb[fn][k])] += 1
    print('=== legacy 有 / IR 无（前 20 形状）===')
    for s, n in del_tab.most_common(20):
        print('%6d  %s' % (n, s))
    print('=== IR 有 / legacy 无（前 20 形状）===')
    for s, n in ins_tab.most_common(20):
        print('%6d  %s' % (n, s))
    print('=== 按文件（前 20）===')
    for f, n in per_file.most_common(20):
        print('%6d  %s.c' % (n, f))


if __name__ == '__main__':
    main()
