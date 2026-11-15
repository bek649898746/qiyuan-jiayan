#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""count_asm —— 从真实 `-S` 产物统计「真实指令数」与目标形态计数（121 轮审计口径）。

口径（路线图 §5.2，踩过 2.43× 的坑后定的）：
  · **指令** = 以空白开头的行（标签 `名字:` 与 `.浮点` 数据指令都不算）；
  · `;` 开头的是注释/元数据（`@I`/`@U`/`@L`/`@QCCMETA`），一律不算；
  · 与 `; @QCCMETA 1 ins=<n>` **交叉验证**（实测两者逐条相等，差 0）；
  · 行数只按 `0x0A` 切（审计判据 7：`.Count` 会少报）。

目标形态（P0/P1/P2/P3 的判据）：
  imm_store   立即数 → 存帧/存静：`移动 r0, N` / `移64 r0, N` 紧跟 `存帧32/64 [..], r0` 或 `存静32/64 [..], r0`
  shl2        `左移 rX, 2`（×4，lea 的主目标）
  mul         `乘`/`乘32双`/`乘64`/`乘无64`（数组下标缩放）
  setcc       `置低/置高/置等/…`（条件位序列的第 2 条）
  zext_al     `零扩展 eax, al`
  test_zero   `测试 r0, r0`
  push/pop    `压栈`/`弹栈`

用法：
  python scripts/count_asm.py <x.asm> [--json] [--samples N]
"""
import argparse
import io
import json
import re
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

META_RE = re.compile(r';\s*@QCCMETA[^i]*ins=(\d+)')
IMM_RE = re.compile(r'^(移动|移64)\s+(r\d+)\s*,\s*(-?\d+)\s*$')
STORE_RE = re.compile(r'^存(帧|静)(32|64)\s+\[[^\]]+\]\s*,\s*(r\d+)\s*$')
SHL2_RE = re.compile(r'^左移(64)?\s+r\d+\s*,\s*2\s*$')
MUL_RE = re.compile(r'^乘')
SETCC_RE = re.compile(r'^置(低|高|等|不等|大|大等|小|小等)')
ZEXT_AL_RE = re.compile(r'^零扩展\s+eax\s*,\s*al\s*$')
TEST_R0_RE = re.compile(r'^测试\s+r0\s*,\s*r0\s*$')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('asm')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--samples', type=int, default=0, help='打印前 N 条 imm_store 配对（残差核验用）')
    ap.add_argument('--func', default=None, help='只统计该函数体（按 `; === 名字 ===` 分段）')
    a = ap.parse_args()

    data = io.open(a.asm, 'rb').read().decode('utf-8', 'surrogateescape')
    lines = [l.rstrip('\r') for l in data.split('\n')]

    if a.func:
        want = '; === %s ===' % a.func
        start = None
        for i, l in enumerate(lines):
            if l.startswith('; === '):
                if start is not None:
                    lines = lines[start:i]
                    break
                if l.startswith(want):
                    start = i + 1
        else:
            if start is None:
                print('[FAIL] 没找到函数 %s' % a.func)
                return 2
            lines = lines[start:]
        if start is None:
            print('[FAIL] 没找到函数 %s' % a.func)
            return 2

    ins = 0
    meta_ins = None
    c = dict(imm_store=0, imm_store_any=0, shl2=0, mul=0, mul_plain=0, setcc=0, zext_al=0,
             test_zero=0, push=0, pop=0)
    store_kind = {}
    pairs = []
    prev = None            # (lineno, mnemonic, reg, imm)
    for i, l in enumerate(lines):
        if not l:
            continue
        if l.startswith(';'):
            m = META_RE.search(l)
            if m and meta_ins is None:
                meta_ins = int(m.group(1))
            continue
        if not l[0].isspace():
            prev = None    # 标签/数据指令打断配对
            continue
        s = l.strip()
        if s.startswith(';'):
            continue
        ins += 1
        op = s.split()[0] if s.split() else ''
        if op in ('压栈',):
            c['push'] += 1
        elif op in ('弹栈',):
            c['pop'] += 1
        if MUL_RE.match(s):
            c['mul'] += 1
        if op == '乘':
            c['mul_plain'] += 1
        if SHL2_RE.match(s):
            c['shl2'] += 1
        if SETCC_RE.match(s):
            c['setcc'] += 1
        if ZEXT_AL_RE.match(s):
            c['zext_al'] += 1
        if TEST_R0_RE.match(s):
            c['test_zero'] += 1
        st = STORE_RE.match(s)
        if st:
            k = st.group(1) + st.group(2)
            store_kind[k] = store_kind.get(k, 0) + 1
            if prev is not None:
                c['imm_store_any'] += 1
            if st and prev is not None:
                pno, preg, pimm = prev
                if st.group(3) == preg:
                    c['imm_store'] += 1
                    if len(pairs) < a.samples:
                        pairs.append((pno + 1, lines[pno].strip(), i + 1, s))
                prev = None
                continue
        mi = IMM_RE.match(s)
        prev = (i, mi.group(2), int(mi.group(3))) if mi else None

    out = dict(ins=ins, meta_ins=meta_ins,
               delta=(None if meta_ins is None else ins - meta_ins), store_kind=store_kind, **c)
    if a.json:
        print(json.dumps(out, ensure_ascii=False))
    else:
        print('ins=%d  meta_ins=%s  delta=%s（应为 0）' % (ins, meta_ins, out['delta']))
        print('imm_store=%d（同寄存器）/ imm_store_any=%d（紧邻任意存帧）'
              % (c['imm_store'], c['imm_store_any']))
        print('shl2=%d  mul=%d（乘* 全族）/ mul_plain=%d（仅「乘」）'
              % (c['shl2'], c['mul'], c['mul_plain']))
        print('setcc=%d  zext_al=%d  test_zero=%d  push=%d  pop=%d'
              % (c['setcc'], c['zext_al'], c['test_zero'], c['push'], c['pop']))
        print('store_kind=%s' % store_kind)
        for pno, pl, sno, sl in pairs:
            print('  L%-8d %-28s\n  L%-8d %s' % (pno, pl, sno, sl))
    return 0 if (meta_ins is None or ins == meta_ins) else 1


if __name__ == '__main__':
    sys.exit(main())
