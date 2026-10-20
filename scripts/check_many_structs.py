#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_many_structs —— many-struct 容量回归判据器（skeptic-1）

历史：round13 的「字段竞技场 4096×64 + 几何倍增」使 64 字段的构实际占 120 条，
2200 个构（≈ 105,600 字段）在 2186 个构处耗尽 262,144 条 ⟹ 误杀合法源
（旧 seed 2421EAAB rc=0 / 运行 rc=7；v2 47318D15 报「字段池满」）。

判据（驱动已发布编译器，源在运行时生成，不硬编码结果）：
  ① 2200 构 × 64 整字段（1.8MB）必须 **编译 rc=0**；
  ② 产物 **运行 rc=7**（读回 `a63 = 7`）；
  ③ 再叠加 200 构 × 130 字段（验证「单类型 >64」路径）仍 rc=0。

用法: python scripts/check_many_structs.py [--cc qcc_zh_v2.exe]
"""
import argparse
import os
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)


def gen(path, nstruct, nfield, use_last_field=63):
    out = []
    for s in range(nstruct):
        out.append('struct S%d {' % s)
        for f in range(nfield):
            out.append('    int a%d;' % f)
        out.append('};')
    out.append('int main(void) {')
    out.append('    struct S%d x = {0};' % (nstruct - 1))
    out.append('    x.a%d = 7;' % use_last_field)
    out.append('    return x.a%d;' % use_last_field)
    out.append('}')
    with open(path, 'w', newline='\n') as f:
        f.write('\n'.join(out) + '\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    a = ap.parse_args()
    tmp = tempfile.mkdtemp(prefix='many_')
    fails = []

    # ① 2200 × 64
    src1 = os.path.join(tmp, 'many_2200x64.c')
    gen(src1, 2200, 64)
    exe1 = os.path.join(tmp, 'many_2200x64.exe')
    p = subprocess.run([a.cc, src1, '-o', exe1], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=600)
    print('[①] 2200 构 × 64 字段: compile rc=%d size=%d' % (p.returncode, os.path.getsize(src1)))
    if p.returncode != 0:
        fails.append('① compile rc=%d: %s' % (p.returncode, (p.stdout or '').strip()[-160:]))
    else:
        r = subprocess.run([exe1], capture_output=True, text=True, timeout=60)
        print('[②] run rc=%d' % r.returncode)
        if r.returncode != 7:
            fails.append('② run rc=%d (期望 7)' % r.returncode)

    # ③ 200 × 130（单类型 >64）
    src2 = os.path.join(tmp, 'many_200x130.c')
    gen(src2, 200, 130, use_last_field=129)
    exe2 = os.path.join(tmp, 'many_200x130.exe')
    p2 = subprocess.run([a.cc, src2, '-o', exe2], capture_output=True, text=True,
                        encoding='utf-8', errors='replace', timeout=600)
    print('[③] 200 构 × 130 字段: compile rc=%d' % p2.returncode)
    if p2.returncode != 0:
        fails.append('③ compile rc=%d: %s' % (p2.returncode, (p2.stdout or '').strip()[-160:]))
    else:
        r2 = subprocess.run([exe2], capture_output=True, text=True, timeout=60)
        print('[③] run rc=%d (期望 7)' % r2.returncode)
        if r2.returncode != 7:
            fails.append('③ run rc=%d (期望 7)' % r2.returncode)

    print()
    if fails:
        print('=== many-struct: RED (%d 项) ===' % len(fails))
        for f in fails:
            print('  - ' + f)
        return 1
    print('=== many-struct: PASS ===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
