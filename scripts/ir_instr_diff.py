# -*- coding: utf-8 -*-
"""逐文件「真实指令差」度量（比 check_ir_byte_parity 的 SAME/DIFF 更细）。

为什么需要：SAME/DIFF 只给「文件是否逐字节相同」。族 A 的修复需要知道
「**还剩几条指令不同**、在哪些函数里」。本脚本编译两模式 → objdump → 按函数切分 →
把 `jmp/call/je/...` 的目标地址规范化（目标变化是尺寸差的**后果**，不是缺陷）→
统计真正不同的指令行数。

用法: python scripts/ir_instr_diff.py --cc build/brs5.exe [--files lstate.c,lapi.c] [--verbose]
"""
import argparse
import io
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = r'D:\qcc_corpus\lua-5.4.7\src'
OBJDUMP = r'C:\mingw-w64\bin\objdump.exe'
WORK = os.path.join(REPO, 'build', 'instrdiff')


def compile_one(cc, path, out, ir):
    env = dict(os.environ)
    if ir:
        env['QCC_CODEGEN'] = 'ir'
    else:
        env.pop('QCC_CODEGEN', None)
    r = subprocess.run([cc, path, '-c', '-o', out], capture_output=True, env=env, cwd=REPO)
    return r.returncode == 0


def dis(path):
    r = subprocess.run([OBJDUMP, '-d', path], capture_output=True)
    text = r.stdout.decode('utf-8', errors='replace')
    cur = None
    d = {}
    for line in text.splitlines():
        if line[:1] in '0123456789abcdef' and '<' in line:
            cur = line.split('<')[1].split('>')[0]
            d.setdefault(cur, [])
            continue
        if cur and '\t' in line:
            t = line.rstrip().split('\t')[-1].strip()
            t = t.split('#')[0].strip()          # 去掉 objdump 的地址注释（含 RIP 相对目标的绝对地址）
            # 目标地址规范化：`jmp 1234 <sym+0x1>` → `jmp <T>`
            t = re.sub(r'\s[0-9a-f]+ <[^>]*>', ' <T>', t)
            d[cur].append(t)
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    ap.add_argument('--files', default='')
    ap.add_argument('--verbose', action='store_true')
    a = ap.parse_args()
    os.makedirs(WORK, exist_ok=True)
    files = ([x.strip() for x in a.files.split(',') if x.strip()]
             if a.files else sorted(os.path.basename(p) for p in
                                    __import__('glob').glob(os.path.join(SRC, '*.c'))))
    total = 0
    rows = []
    for f in files:
        p = os.path.join(SRC, f)
        lo = os.path.join(WORK, f + '.leg.obj')
        io_ = os.path.join(WORK, f + '.ir.obj')
        if not (compile_one(a.cc, p, lo, False) and compile_one(a.cc, p, io_, True)):
            rows.append((f, 'COMPILE-FAIL', 0))
            continue
        dl, di = dis(lo), dis(io_)
        diffn = 0
        details = []
        for k in dl:
            if k not in di:
                continue
            if dl[k] != di[k]:
                # 求最小差异行数
                import difflib
                sm = difflib.SequenceMatcher(None, dl[k], di[k], autojunk=False)
                n = sum(max(i2 - i1, j2 - j1) for tag, i1, i2, j1, j2 in sm.get_opcodes() if tag != 'equal')
                diffn += n
                details.append((k, n))
        total += diffn
        rows.append((f, 'SAME' if diffn == 0 else 'DIFF', diffn, details))
    print('%-14s %-8s %s' % ('文件', '状态', '真实指令差'))
    for r in rows:
        print('%-14s %-8s %d' % (r[0], r[1], r[2]))
        if a.verbose and len(r) > 3 and r[3]:
            for k, n in sorted(r[3], key=lambda x: -x[1])[:8]:
                print('      %-32s %d' % (k, n))
    print('合计真实指令差 = %d' % total)
    return 0 if total == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
