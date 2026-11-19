#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""IR 侧「逐函数字节尺」—— 主源两路径（legacy / QCC_CODEGEN=ir）的函数级字节数对账。

为什么需要它：AC3 要求主源 `.o` 两路径**逐字节相同**。整文件比字节只能给出「不相同」，
无法指出「哪些函数、差多少」。本尺子按 COFF 符号边界切函数，逐函数比字节数，输出：
  · 差异函数数 / Σdelta（带符号）
  · 头部正/负差额函数
  · `push %r11` 条数对比（§717 实测：剩余差额约 90% 是多出的 `push %r11`）

用法（默认编译主源，跑真路径；不写死任何数字）：
    python scripts\\check_ir_fn_size.py
    python scripts\\check_ir_fn_size.py --cc qcc_zh_k1112.exe --src srclib_jiayan\\qcc_zh.jy
    python scripts\\check_ir_fn_size.py --obj build\\leg.o build\\ir.o   # 复用已有 .o

证据落 {scratch}/ir_fn_size.log（由调用方决定 stdout 去向），脚本本身只打印。
"""
import argparse
import io
import os
import re
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def coff_functions(path):
    """COFF(.o) 里 name -> (起始, 长度)。长度 = 下一个函数起点 − 本函数起点。"""
    buf = io.open(path, 'rb').read()
    nsec = struct.unpack_from('<H', buf, 2)[0]
    symoff = struct.unpack_from('<I', buf, 8)[0]
    nsym = struct.unpack_from('<I', buf, 12)[0]
    optsz = struct.unpack_from('<H', buf, 16)[0]
    base = 20 + optsz
    text_size = 0
    for i in range(nsec):
        b = base + i * 40
        nm = buf[b:b + 8].rstrip(b'\0').decode('latin1')
        vs, vma, rs, rp = struct.unpack_from('<IIII', buf, b + 8)
        if nm == '.text':
            text_size = vs
    strtab = symoff + nsym * 18

    def name_of(e):
        if e[0:4] == b'\0\0\0\0':
            off = struct.unpack_from('<I', e, 4)[0]
            end = buf.find(b'\0', strtab + off)
            return buf[strtab + off:end].decode('utf-8', 'replace')
        return e[0:8].rstrip(b'\0').decode('utf-8', 'replace')

    funcs = []
    for k in range(nsym):
        e = buf[symoff + k * 18: symoff + k * 18 + 18]
        if len(e) < 18:
            break
        value, secnum, typ, sclass, naux = struct.unpack_from('<IhHBB', e, 8)
        if secnum != 1 or (typ & 0x20) == 0:
            continue
        funcs.append((value, name_of(e)))
    funcs.sort()
    out = {}
    for i, (v, n) in enumerate(funcs):
        nxt = funcs[i + 1][0] if i + 1 < len(funcs) else text_size
        out[n] = (v, nxt - v)
    return out


def push11(path, funcname, cache={}):
    """数某函数里 `push %r11` 的条数（objdump 反汇编，剥 # 注释与 rex 前缀）。"""
    if path not in cache:
        out = subprocess.run(['objdump', '-d', '--no-show-raw-insn', path],
                             capture_output=True).stdout.decode('utf-8', 'replace')
        funcs = {}
        cur = None
        for line in out.splitlines():
            m = re.match(r'^[0-9a-f]+ <(.+)>:$', line.strip())
            if m:
                cur = m.group(1)
                funcs[cur] = []
                continue
            m = re.match(r'^\s+[0-9a-f]+:\t(.*)$', line.rstrip())
            if m and cur:
                ins = re.sub(r'\s+', ' ', re.sub(r'^rex ', '', m.group(1).split('#')[0].strip()))
                funcs[cur].append(ins)
        cache[path] = funcs
    return sum(1 for x in cache[path].get(funcname, []) if x == 'push %r11')


def build(cc, src, out, ir):
    env = dict(os.environ)
    if ir:
        env['QCC_CODEGEN'] = 'ir'
    else:
        env.pop('QCC_CODEGEN', None)
    r = subprocess.run([cc, src, '-c', '-o', out], env=env, capture_output=True)
    if r.returncode != 0:
        sys.stderr.write(r.stderr.decode('utf-8', 'replace'))
        raise SystemExit('[FAIL] 编译失败 rc=%d: %s' % (r.returncode, out))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--src', default=os.path.join('srclib_jiayan', 'qcc_zh.jy'))
    ap.add_argument('--obj', nargs=2, metavar=('LEG_O', 'IR_O'),
                    help='跳过编译，直接用两份已有 .o')
    ap.add_argument('--work', default=None)
    a = ap.parse_args()

    if a.obj:
        leg_o, ir_o = a.obj
    else:
        cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
        src = a.src if os.path.isabs(a.src) else os.path.join(ROOT, a.src)
        work = a.work or os.environ.get('TEMP') or '.'
        leg_o = build(cc, src, os.path.join(work, 'fn_size_leg.o'), False)
        ir_o = build(cc, src, os.path.join(work, 'fn_size_ir.o'), True)

    a_f, b_f = coff_functions(leg_o), coff_functions(ir_o)
    common = [k for k in a_f if k in b_f]
    rows = [(b_f[k][1] - a_f[k][1], k, a_f[k][1], b_f[k][1]) for k in common
            if a_f[k][1] != b_f[k][1]]
    rows.sort()
    print('== IR 逐函数字节尺 ==')
    print('legacy .o : %s' % leg_o)
    print('IR     .o : %s' % ir_o)
    print('函数总数 %d · 差异函数 %d · Σdelta %+d 字节' %
          (len(common), len(rows), sum(r[0] for r in rows)))
    print('-- 正差额（IR 更大）top 8 --')
    for d, k, la, lb in rows[::-1][:8]:
        print('  %+7d  %-18s %7d -> %7d   push11 %d/%d' %
              (d, k, la, lb, push11(leg_o, k), push11(ir_o, k)))
    print('-- 负差额（IR 更小）top 8 --')
    for d, k, la, lb in rows[:8]:
        print('  %+7d  %-18s %7d -> %7d   push11 %d/%d' %
              (d, k, la, lb, push11(leg_o, k), push11(ir_o, k)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
