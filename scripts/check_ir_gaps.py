#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ir_gaps —— 三条结构缺口判据（施工单 `_施工单_IR路线_单一入口` §七 步 7）。

缺口 1 · 调用实参进 L1：`抽调` 必须带实参记录（子指令链逐条 `抽实参`，或 源二 非 0）。
缺口 2 · 六种比较可区分：`a<b` / `a<=b` / `a>b` / `a>=b` / `a==b` / `a!=b` 的
          `抽整比` 记录，`现高` 的属性位（位 10-12）必须两两不同。
缺口 3 · 指针加减可区分：`p+n` 与 `p-n` 的 `抽指针加` 记录，位 13 必须不同。

用法: python scripts/check_ir_gaps.py [--cc qcc_zh_v1.exe] [--work <dir>]
退出码: 0 全过 ; 1 有失败
"""
import argparse
import io
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
try:
    from qcc_tmp import tmp_dir
except Exception:
    tmp_dir = lambda n: os.path.join('build', n)

# 缺口 2：六种比较（C 源 → 期望属性值 1..6，与 比较属性() 一致）
CMP = [
    ('lt', 'a < b', 1),
    ('le', 'a <= b', 2),
    ('gt', 'a > b', 3),
    ('ge', 'a >= b', 4),
    ('eq', 'a == b', 5),
    ('ne', 'a != b', 6),
]
CMP_SRC = 'int f(int a, int b) { return %s; }\n'

PTR_ADD_SRC = 'int f(int *p, int n) { return p + n; }\n'
PTR_SUB_SRC = 'int f(int *p, int n) { return p - n; }\n'
ARG_SRC = 'int h(int a) { return a; }\nint g(void) { return h(7); }\n'

RE_ABBR = re.compile(r'抽\[(\d+)\]\s+算子=(\d+)\s+现高=(-?\d+)\s+元=(-?\d+)\s+源一=(-?\d+)\s+源二=(-?\d+)\s+链头=(-?\d+)')


def run_ir(cc, src_path, work):
    env = dict(os.environ)
    env['QCC_DUMP'] = 'ir'
    env.pop('QCC_CODEGEN', None)
    r = subprocess.run([cc, src_path, '-c', '-o', os.path.join(work, 'gap.obj')],
                       capture_output=True, env=env, timeout=600)
    return r, (r.stdout + r.stderr).decode('utf-8', 'replace')


def abbrs(out):
    return [tuple(int(x) for x in m.groups()) for m in RE_ABBR.finditer(out)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--work', default=None)
    a = ap.parse_args()
    if not os.path.exists(a.cc):
        print('环境未就绪：缺 %s' % a.cc)
        return 2
    W = a.work or tmp_dir('ir_gaps')
    os.makedirs(W, exist_ok=True)
    fails = []

    def write(name, text):
        p = os.path.join(W, name)
        io.open(p, 'w', encoding='utf-8', newline='\n').write(text)
        return p

    # ── 缺口 2 · 比较可区分 ─────────────────────────────────────────────
    seen = {}
    for tag, expr, want in CMP:
        sp = write('cmp_%s.c' % tag, CMP_SRC % expr)
        r, out = run_ir(a.cc, sp, W)
        if r.returncode != 0:
            fails.append('GAP2 %s 编译失败 rc=%d' % (tag, r.returncode))
            continue
        rows = [x for x in abbrs(out) if x[1] == 1031]  # 抽整比
        if not rows:
            fails.append('GAP2 %s 无 抽整比(1031) 记录' % tag)
            continue
        attr = (rows[-1][2] >> 10) & 7
        seen[tag] = attr
        if attr != want:
            fails.append('GAP2 %s 属性=%d 期望=%d（现高=%d）' % (tag, attr, want, rows[-1][2]))
    if len(set(seen.values())) == len(seen) and len(seen) == 6 and not any('GAP2' in f for f in fails):
        print('GAP2 六种比较可区分: PASS (%s)' % ' '.join('%s=%d' % (k, v) for k, v in seen.items()))

    # ── 缺口 3 · 指针加减可区分 ─────────────────────────────────────────
    marks = {}
    for tag, src in (('add', PTR_ADD_SRC), ('sub', PTR_SUB_SRC)):
        sp = write('ptr_%s.c' % tag, src)
        r, out = run_ir(a.cc, sp, W)
        if r.returncode != 0:
            fails.append('GAP3 %s 编译失败 rc=%d' % (tag, r.returncode))
            continue
        rows = [x for x in abbrs(out) if x[1] == 1101]  # 抽指针加
        if not rows:
            fails.append('GAP3 %s 无 抽指针加(1101) 记录' % tag)
            continue
        marks[tag] = (rows[-1][2] >> 13) & 1
    if 'add' in marks and 'sub' in marks:
        if marks['add'] == marks['sub']:
            fails.append('GAP3 p+n 与 p-n 位13 相同（%d）⟹ 不可区分' % marks['add'])
        else:
            print('GAP3 指针加减可区分: PASS (add bit13=%d, sub bit13=%d)'
                  % (marks['add'], marks['sub']))

    # ── 缺口 1 · 调用实参进 L1 ──────────────────────────────────────────
    sp = write('arg.c', ARG_SRC)
    r, out = run_ir(a.cc, sp, W)
    if r.returncode != 0:
        fails.append('GAP1 编译失败 rc=%d' % r.returncode)
    else:
        rows = [x for x in abbrs(out) if x[1] == 1071]  # 抽调
        if not rows:
            fails.append('GAP1 无 抽调(1071) 记录')
        else:
            call = rows[-1]
            # 子指令链头 >= 0 表示有实参记录；源二 非 0 是兼容形态
            if call[6] < 0 and call[5] == 0:
                fails.append('GAP1 调用 抽[%d] 无实参记录（源二=0 且 链头=-1）' % call[0])
            else:
                print('GAP1 调用实参进 L1: PASS (抽[%d] 源二=%d 链头=%d)'
                      % (call[0], call[5], call[6]))

    if fails:
        print('\n[失败 FAIL] ' + '；'.join(fails))
        return 1
    print('\n[通过 PASS] 三条结构缺口全绿（实参 / 比较 / 指针加减）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
