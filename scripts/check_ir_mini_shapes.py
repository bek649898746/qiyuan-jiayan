# -*- coding: utf-8 -*-
"""最小形态矩阵判据：7 类成员/指针/下标形状，两模式逐字节对比。

用途：给「−3 / −20 字节」这类**形态差**一个秒级、可复现的定位台；
语料里的大函数（lfunc/lvm）只告诉我们「哪一字节不同」，这里告诉我们**是哪一类形状**。

ratchet：`scripts/ir_mini_baseline.txt` 列出当前已知 DIFF 的形状名；
* 基线里的形状转 SAME ⟹ 提示可以删基线行（不算失败，便于收账）；
* 不在基线里的形状变 DIFF ⟹ **失败**（新回归）。

用法: python scripts/check_ir_mini_shapes.py [--cc qcc_zh_v2.exe]
"""
import argparse
import io
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(REPO, 'scripts', 'ir_mini_baseline.txt')

CASES = {
    'ptr_member': 'struct T { int a; long b; };\nstruct T g;\nstruct T *p;\nlong f(void) { return p->b; }\n',
    'val_member': 'struct T { int a; long b; };\nstruct T g;\nlong f(void) { return g.b; }\n',
    'nested_ptr': 'struct U { struct T { int a; long b; } t; };\nstruct U *q;\nlong f(void) { return q->t.b; }\n',
    'addrof_member': 'struct T { int a; long b; };\nstruct T g;\nlong *f(void) { return &g.b; }\n',
    'local_ptr_chain': 'struct T { int a; long b; };\nstruct T g;\nlong f(void) { struct T *p = &g; return p->b; }\n',
    'local_ptr_one': 'struct T { long b; };\nstruct T g;\nlong f(void) { struct T *p = &g; return p->b; }\n',
    'addrof_arrow': 'struct T { int a; long b; };\nstruct T g;\nlong f(void) { return (&g)->b; }\n',
}


def build(cc, src, out, ir):
    env = dict(os.environ)
    if ir:
        env['QCC_CODEGEN'] = 'ir'
    else:
        env.pop('QCC_CODEGEN', None)
    return subprocess.run([cc, src, '-c', '-o', out], capture_output=True, env=env, cwd=REPO).returncode == 0


OBJCOPY = r'C:\mingw-w64\bin\objcopy.exe'


def text_bytes(obj):
    """只比 `.text` —— 整个 .o 的差异包含重定位/节头，属正常的地址布局后果。"""
    out = obj + '.text.bin'
    subprocess.run([OBJCOPY, '--only-section=.text', '-O', 'binary', obj, out], check=True)
    b = io.open(out, 'rb').read()
    os.remove(out)
    return b


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    a = ap.parse_args()
    work = os.path.join(REPO, 'build', 'mini_shapes')
    os.makedirs(work, exist_ok=True)
    base = set()
    if os.path.exists(BASE):
        base = set(x.strip() for x in io.open(BASE, encoding='utf-8') if x.strip() and not x.startswith('#'))
    bad = []
    for name, code in sorted(CASES.items()):
        src = os.path.join(work, name + '.c')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(code)
        lo = os.path.join(work, name + '.leg.o')
        ir = os.path.join(work, name + '.ir.o')
        if not (build(a.cc, src, lo, False) and build(a.cc, src, ir, True)):
            print('  %-18s COMPILE-FAIL' % name)
            bad.append('%s 编译失败' % name)
            continue
        same = text_bytes(lo) == text_bytes(ir)
        print('  %-18s %s' % (name, 'SAME' if same else 'DIFF'))
        if not same and name not in base:
            bad.append('%s 新 DIFF（不在基线）' % name)
        if same and name in base:
            print('     （基线里有、现已 SAME ⟹ 可删除该基线行）')
    if bad:
        print('[FAIL] check_ir_mini_shapes: %d 项' % len(bad))
        for b in bad:
            print('   - ' + b)
        sys.exit(1)
    print('[PASS] check_ir_mini_shapes：%d 类形状与基线一致' % len(CASES))


main()
