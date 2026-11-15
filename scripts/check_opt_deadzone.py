# -*- coding: utf-8 -*-
"""check_opt_deadzone.py —— ④乙 恒跳死区判据（ORD-20261110-DEADZONE）.

三项：
  A. `若(0){40 条}` 样本：默认 vs `--no-4b`，`.text` 必须小 ≥1000 B（审计实测可清 1,492 B）；
  B. 负控：活条件 `if (g != 1){同样的 40 条}` ⟹ **不得**触发死区（输出里没有 `[死区]` 行），
     且产物与 `--no-4b` 逐字节相同；
  C. 行为：死区样本清理前后运行，stdout 与退出码必须一致。

用法: python scripts/check_opt_deadzone.py [--cc qcc_zh_v1.exe] [--opt opt_zh.exe]
退出码: 0 = PASS / 1 = FAIL / 2 = 环境未就绪
"""
import argparse
import io
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402
import re

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

BODY = '\n'.join('        g = g * 3 + %d;' % i for i in range(1, 41))
DEAD_SRC = '#include <stdio.h>\nint main(void) {\n    int g = 1;\n    if (0) {\n%s\n    }\n    printf("%%d\\n", g);\n    return 0;\n}\n' % BODY
LIVE_SRC = DEAD_SRC.replace('if (0) {', 'if (g != 1) {')


def run(cmd, timeout=1800):
    r = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=timeout)
    return r.returncode, (r.stdout or '') + (r.stderr or '')


def code_bytes_from_asm(out):
    """汇编器自报 `(N bytes code ...)` —— PE 段 raw 按 512 对齐，会掩盖 ~1.3KB 的差。"""
    m = re.search(r'\((\d+) bytes code', out)
    return int(m.group(1)) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--opt', default='opt_zh.exe')
    a = ap.parse_args()
    for p in (a.cc, a.opt, 'asm_zh.exe'):
        if not os.path.exists(p):
            print('环境未就绪：缺 %s' % p)
            return 2

    work = tmp_dir('opt_deadzone')
    bad = []

    def build(tag, src_text):
        src = os.path.join(work, tag + '.c')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(src_text)
        exe = os.path.join(work, tag + '.exe')
        rc, out = run([a.cc, src, '-o', exe])
        if rc != 0:
            bad.append('%s 编译失败 rc=%d' % (tag, rc))
            return None
        return exe, exe + '.asm_src'

    def opt_assemble(tag, asm_src, extra):
        asm = os.path.join(work, tag + '_opt.asm')
        rc, out = run([a.opt, asm_src, '-o', asm] + extra)
        hit = sum(1 for l in out.splitlines() if l.startswith('[死区]'))
        if rc != 0:
            bad.append('%s opt rc=%d' % (tag, rc))
            return None, hit, out
        exe = os.path.join(work, tag + '_opt.exe')
        rc2, out2 = run(['asm_zh.exe', asm, '-o', exe])
        if rc2 != 0:
            bad.append('%s asm rc=%d' % (tag, rc2))
            return None, hit, out
        return exe, hit, out + out2, code_bytes_from_asm(out2)

    # A: 死区样本
    built = build('dead40', DEAD_SRC)
    if built is None:
        print('[失败 FAIL] ' + '；'.join(bad))
        return 1
    dsrc = built[0]
    # -S 生成 .asm
    rc, out = run([a.cc, os.path.join(work, 'dead40.c'), '-S', '-o', os.path.join(work, 'dead40s.exe')])
    asm_src = os.path.join(work, 'dead40s.exe.asm')
    if rc != 0 or not os.path.exists(asm_src):
        bad.append('dead40 -S 失败 rc=%d' % rc)
    exe_new, hit_new, _, cb_new = opt_assemble('dead_new', asm_src, [])
    exe_old, hit_old, _, cb_old = opt_assemble('dead_old', asm_src, ['--no-4b'])
    if exe_new is None or exe_old is None:
        print('[失败 FAIL] ' + '；'.join(bad))
        return 1
    sz_new, sz_old = cb_new, cb_old
    delta = sz_old - sz_new
    print('A 死区: 默认 命中区间=%d · .text %d -> %d（-%d B）' % (hit_new, sz_old, sz_new, delta))
    if hit_new < 1:
        bad.append('A 未触发死区（命中可删区间 = 0）')
    if delta < 1000:
        bad.append('A 清理量 %d B < 1000 B' % delta)

    # B: 负控（活条件）
    io.open(os.path.join(work, 'live40.c'), 'w', encoding='utf-8', newline='\n').write(LIVE_SRC)
    rc, out = run([a.cc, os.path.join(work, 'live40.c'), '-S', '-o', os.path.join(work, 'live40s.exe')])
    lasm = os.path.join(work, 'live40s.exe.asm')
    if rc != 0 or not os.path.exists(lasm):
        bad.append('live40 -S 失败 rc=%d' % rc)
    else:
        lnew, lhit, _, _ = opt_assemble('live_new', lasm, [])
        lold, _, _, _ = opt_assemble('live_old', lasm, ['--no-4b'])
        if lnew and lold:
            same = io.open(lnew, 'rb').read() == io.open(lold, 'rb').read()
            print('B 负控: 死区命中=%d（应 0） · 产物与 --no-4b %s' % (lhit, '逐字节相同' if same else '**不同**'))
            if lhit != 0:
                bad.append('B 活条件被误判为死区（命中 %d）' % lhit)
            if not same:
                bad.append('B 负控产物与 --no-4b 不同')

    # C: 行为一致
    def runtime(p):
        r = subprocess.run([p], capture_output=True)
        return r.returncode, r.stdout
    ra, rb = runtime(exe_new), runtime(exe_old)
    print('C 行为: 清理后 %r vs 清理前 %r' % (ra, rb))
    if ra != rb:
        bad.append('C 清理前后运行结果不同')

    print()
    if bad:
        print('[失败 FAIL] check_opt_deadzone：')
        for b in bad:
            print('  - ' + b)
        return 1
    print('[通过 PASS] check_opt_deadzone：死区清理 %d B（>=1000）· 负控 0 误删 · 行为一致' % delta)
    return 0


if __name__ == '__main__':
    sys.exit(main())
