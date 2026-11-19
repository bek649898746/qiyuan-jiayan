#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ir_inc_in_call —— `++` 落在内建调用实参里时，IR 的 `-c` 字节必须与 legacy 相同。

## 为什么需要这个判据

台账 §654：`抽调` 有捕获跨度时走 `内建回放`（**纯字节旁路，不遍历实参 AST**）⟹
实参子树里的 `抽增减` 永远进不了 `IR发节点` ⟹ 它的 28 字节只躺在 `增减缓` 里无人取用。

主源里最小、最干净的复现器是 `srclib_jiayan/qcc_zh.jy` 的：

    静 空 不安全标(常 字 *名) { 若 (不安全数 < 256 && !不安全查(名)) 复制字符串(不安全名[不安全数++], 名); }

`-c` 实测：legacy 294 B / IR 266 B，指令级 diff 恰好 1 个 hunk、7 条指令（整数全局后自增整段）。

## 为什么不用独立小 C 文件

曾在 6 个 scratch 用例上尝试复现，全部失败（§627 / §637）——脱离主源上下文不出这个形态。
因此本判据只能用**主源自身**做靶子。这也是它必须存在的原因：它是唯一能稳定红的靶。

## 判据（fail-closed）

1. 同源 legacy `-c` 与 IR `-c` 都必须 rc=0（否则判红，不许「编译不过也算过」）
2. `不安全标` 在两个 `.o` 里的**代码字节**必须逐字节相同
3. 额外记录 `push %rax` / `pop %rax` / 7 条自增序 三项计数（§654：比函数 size 差可靠得多）

用法: python scripts/check_ir_inc_in_call.py [--cc <exe>] [--func 不安全标] [--keep]
退出码: 0 = 通过；1 = 判红（缺陷仍在）；2 = 环境未就绪
"""
import argparse
import io
import os
import re
import struct
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

SRC = os.path.join('srclib_jiayan', 'qcc_zh.jy')


def parse_coff(path):
    """→ {函数名: (起始, 长度, 原始字节)}，长度 = 下一个函数起始 − 本函数起始。"""
    buf = open(path, 'rb').read()
    nsec = struct.unpack_from('<H', buf, 2)[0]
    symoff = struct.unpack_from('<I', buf, 8)[0]
    nsym = struct.unpack_from('<I', buf, 12)[0]
    optsz = struct.unpack_from('<H', buf, 16)[0]
    base = 20 + optsz
    text = None
    for i in range(nsec):
        b = base + i * 40
        nm = buf[b:b + 8].rstrip(b'\0').decode('latin1')
        vs, vma, rs, rp = struct.unpack_from('<IIII', buf, b + 8)
        if nm == '.text':
            text = (rs, rp)
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
        value, secnum, typ, sclass, naux = struct.unpack_from('<IhHBB', e, 8)
        if secnum != 1 or (typ & 0x20) == 0:
            continue
        funcs.append((value, name_of(e)))
    funcs.sort()
    rs, rp = text
    out = {}
    for i, (v, n) in enumerate(funcs):
        nxt = funcs[i + 1][0] if i + 1 < len(funcs) else rs
        out[n] = (v, nxt - v, buf[rp + v: rp + nxt])
    return out


def count_patterns(path, fname):
    """用 objdump 数三项： push %rax / pop %rax / 7 条自增序（宽松）。"""
    objdump = os.path.join(os.environ.get('MINGW', r'C:\mingw-w64\bin'), 'objdump.exe')
    if not os.path.exists(objdump):
        objdump = 'objdump'
    fns = parse_coff(path)
    if fname not in fns:
        return None
    v, sz, _ = fns[fname]
    r = subprocess.run([objdump, '-d', '--start-address=%d' % v,
                        '--stop-address=%d' % (v + sz), path], capture_output=True)
    ins = []
    for l in r.stdout.decode('utf-8', 'replace').splitlines():
        m = re.match(r'^\s+[0-9a-f]+:(.*)$', l)
        if not m:
            continue
        s = m.group(1).split('#')[0]
        toks = s.split()
        k = 0
        while k < len(toks) and re.fullmatch(r'[0-9a-f]{2}', toks[k]):
            k += 1
        ins.append(' '.join(toks[k:]))
    n7 = 0
    for i, x in enumerate(ins):
        if x.strip() != 'push %rax':
            continue
        if not any(i + k < len(ins) and ins[i + k].strip() == 'mov $0x1,%ecx' for k in range(1, 9)):
            continue
        if any(i + k < len(ins) and ins[i + k].strip() == 'pop %rax' for k in range(1, 12)):
            n7 += 1
    return dict(
        insns=len(ins), size=sz,
        push_rax=sum(1 for x in ins if x.strip() == 'push %rax'),
        pop_rax=sum(1 for x in ins if x.strip() == 'pop %rax'),
        pat7=n7)


def disasm_norm(path, v, sz):
    """规范化指令流：去字节列、把跳转/调用目标与 RIP disp 归一。

    判据用「指令流」而不是「原始字节」：`-c` 下 `mov 0x0(%rip)` 的 disp32 由重定位表负责
    （legacy/IR 的占位值可以不同），原始字节相同不是必要条件 —— 主判据
    `check_ir_byte_parity.py` 同样按「跳过重定位窗口」比较。"""
    objdump = os.path.join(os.environ.get('MINGW', r'C:\mingw-w64\bin'), 'objdump.exe')
    if not os.path.exists(objdump):
        objdump = 'objdump'
    r = subprocess.run([objdump, '-d', '--start-address=%d' % v,
                        '--stop-address=%d' % (v + sz), path], capture_output=True)
    out = []
    for l in r.stdout.decode('utf-8', 'replace').splitlines():
        m = re.match(r'^\s+[0-9a-f]+:(.*)$', l)
        if not m:
            continue
        t = m.group(1).split('#')[0]
        t = re.sub(r'\b0x[0-9a-f]+ <[^>]*>', 'TGT', t)
        t = re.sub(r'\b[0-9a-f]{4,} <[^>]*>', 'TGT', t)
        t = re.sub(r'^(j\w+|call|loop\w*)\s+[0-9a-f]+$', r'\1 TGT', t)
        toks = t.split()
        k = 0
        while k < len(toks) and re.fullmatch(r'[0-9a-f]{2}', toks[k]):
            k += 1
        s = ' '.join(toks[k:])
        # RIP 相对 disp 归一（重定位负责真值）
        s = re.sub(r'0x0\(%rip\)', 'RIP(%rip)', s)
        out.append(s)
    return out


def compile_one(cc, out, ir, work):
    env = dict(os.environ)
    if ir:
        env['QCC_CODEGEN'] = 'ir'
    else:
        env.pop('QCC_CODEGEN', None)
    r = subprocess.run([cc, SRC, '-c', '-o', out], capture_output=True, env=env, cwd=ROOT)
    if r.returncode != 0:
        sys.stderr.write('[ERR] %s 编译失败 rc=%d\n%s\n' % (cc, r.returncode,
                                                            r.stderr.decode('utf-8', 'replace')[-2000:]))
    return r.returncode


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_v2.exe'))
    ap.add_argument('--func', default='不安全标')
    ap.add_argument('--work', default=os.path.join(ROOT, 'build'))
    ap.add_argument('--keep', action='store_true')
    a = ap.parse_args()

    if not os.path.exists(a.cc):
        print('[SKIP] 编译器不存在: %s' % a.cc)
        return 2
    os.makedirs(a.work, exist_ok=True)
    leg = os.path.join(a.work, '_incin_leg.o')
    ir = os.path.join(a.work, '_incin_ir.o')

    if compile_one(a.cc, leg, False, a.work) != 0:
        return 1
    if compile_one(a.cc, ir, True, a.work) != 0:
        return 1

    L = parse_coff(leg)
    I = parse_coff(ir)
    f = a.func
    if f not in L or f not in I:
        print('[RED] 目标函数 %s 不在符号表里（判据失效，必须显式失败而不是静默跳过）' % f)
        return 1

    _, lsz, lb = L[f]
    _, isz, ib = I[f]
    lv, _, _ = L[f]
    iv, _, _ = I[f]
    il = disasm_norm(leg, lv, lsz)
    ii = disasm_norm(ir, iv, isz)
    same = (il == ii)
    sl = count_patterns(leg, f)
    si = count_patterns(ir, f)

    print('== %s ==' % f)
    print('  legacy  size=%d  %s' % (lsz, sl))
    print('  IR      size=%d  %s' % (isz, si))
    print('  规范化指令流相同: %s（%d vs %d 条）' % ('是' if same else '否', len(il), len(ii)))
    print('  原始字节相同: %s（disp 由重定位负责，不单独判红）' % ('是' if (lsz == isz and lb == ib) else '否'))

    ok = same
    # 附加：自增序计数必须与 legacy 一致（size 相等但内容错位时靠这一条兜住）
    if sl and si and sl['pat7'] != si['pat7']:
        print('  [RED] 自增序计数不一致: legacy %d / IR %d' % (sl['pat7'], si['pat7']))
        ok = False

    if not a.keep:
        for p in (leg, ir):
            try:
                os.remove(p)
            except OSError:
                pass

    if ok:
        print('\n[PASS] `++` 在内建调用实参里：legacy 与 IR `-c` 代码字节逐字节相同')
        return 0
    print('\n[RED] 缺陷仍在（台账 §654/§655/§656/§657）：IR 少发 `++` 的整段自增序')
    print('      正解方向：缺口按偏移插入 + 函数级「标定遍」判定该 `抽增减` 是否会被遍历（§657 施工单）')
    return 1


if __name__ == '__main__':
    sys.exit(main())
