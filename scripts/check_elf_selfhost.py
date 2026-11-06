#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_elf_selfhost —— Linux 侧自举判据（甲言在 Linux 上编译自己）。

五项（全部真跑，不读源码旁证）：
  1. `qcc_zh_v2.exe --target=elf srclib_jiayan/qcc_zh.jy` → ELF 版编译器，rc=0 且魔数为 ELF64；
  2. `qcc_zh_elf --help` rc=0 且输出非空；
  3. \qcc_zh_elf tiny.c -o tiny.elf\（**不带 --target**，按 ELF 宿主默认目标）→ rc=0，产物为 ELF64 可执行，输出 \	iny-ok 42\（与 gcc 同源对照）；
  4. `qcc_zh_elf srclib_jiayan/qcc_zh.jy -o v1`（**不带 --target**）→ rc=0，`v1` 为 ELF64，`v1 --help` rc=0；
  5. `v1` 编 → `v2`，`v2` 编 → `v3`，三步 rc=0，且 **sha256(v1) == sha256(v2) == sha256(v3)**（Linux 固定点）。

用法: python scripts/check_elf_selfhost.py [--keep <dir>]
退出码: 0 = 五项全过；1 = 有红；2 = 环境未就绪（无 WSL / 无 qcc_zh_v2.exe）
"""
import argparse
import hashlib
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

CC = os.path.join(ROOT, 'qcc_zh_v2.exe')
MAIN_SRC = 'srclib_jiayan/qcc_zh.jy'
TINY = '/* selfhost probe */\n#include <stdio.h>\nint main(void) { printf("tiny-ok %d\\n", 41 + 1); return 0; }\n'


def wsl_path(p):
    p = p.replace('\\', '/')
    if len(p) > 1 and p[1] == ':':
        return '/mnt/' + p[0].lower() + p[2:]
    return p


def wsl(cmd, timeout=1800):
    r = subprocess.run(['wsl', 'bash', '-lc', 'cd %s && %s' % (wsl_path(ROOT), cmd)],
                       capture_output=True, timeout=timeout)
    return r.returncode, (r.stdout or b'').decode('utf-8', 'replace'), (r.stderr or b'').decode('utf-8', 'replace')


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--keep', default=None, help='中间产物目录（默认临时目录）')
    a = ap.parse_args()

    if not os.path.exists(CC):
        print('[SKIP] 环境未就绪：缺 qcc_zh_v2.exe')
        return 2
    try:
        if subprocess.run(['wsl', 'true'], capture_output=True, timeout=30).returncode != 0:
            print('[SKIP] 环境未就绪：WSL 不可用')
            return 2
    except Exception as e:
        print('[SKIP] 环境未就绪：WSL 不可用 (%s)' % e)
        return 2

    work = a.keep or os.path.join(os.environ.get('TEMP', '.'), 'qcc_selfhost')
    os.makedirs(work, exist_ok=True)
    tiny = os.path.join(work, 'tiny.c')
    io.open(tiny, 'w', encoding='utf-8', newline='\n').write(TINY)

    W = wsl_path(work)
    red = []

    # 1) 产 ELF 版编译器
    elfcc = os.path.join(work, 'qcc_zh_elf')
    r = subprocess.run([CC, '--target=elf', MAIN_SRC, '-o', elfcc], capture_output=True)
    ok1 = r.returncode == 0 and os.path.exists(elfcc) and io.open(elfcc, 'rb').read(4) == b'\x7fELF'
    print('[%s] 1 产 ELF 版编译器 rc=%d 魔数=%s' % ('PASS' if ok1 else 'FAIL', r.returncode,
          'ELF' if os.path.exists(elfcc) and io.open(elfcc, 'rb').read(4) == b'\x7fELF' else '?'))
    if not ok1:
        red.append('1 产 ELF 版编译器')

    # 2) --help
    rc, out, err = wsl('"%s" --help' % wsl_path(elfcc))
    ok2 = rc == 0 and out.strip() != ''
    print('[%s] 2 --help rc=%d stdout_len=%d' % ('PASS' if ok2 else 'FAIL', rc, len(out)))
    if not ok2:
        red.append('2 --help')

    # 3) tiny 闭环 + gcc 对照
    rc, out, err = wsl('rm -f %s/tiny.elf; "%s" %s/tiny.c -o %s/tiny.elf; echo "E=$?"; %s/tiny.elf; echo "R=$?"'
                       % (W, wsl_path(elfcc), W, W, W))
    rc2, gout, _ = wsl('gcc -w %s/tiny.c -o %s/tiny_gcc && %s/tiny_gcc' % (W, W, W))
    jy_out = [l for l in out.splitlines() if l.startswith('tiny-ok')]
    gcc_out = [l for l in gout.splitlines() if l.startswith('tiny-ok')]
    tiny_elf = os.path.join(work, 'tiny.elf')
    tiny_is_elf = os.path.exists(tiny_elf) and io.open(tiny_elf, 'rb').read(4) == b'\x7fELF'
    ok3 = ('E=0' in out) and ('R=0' in out) and jy_out == gcc_out and jy_out == ['tiny-ok 42'] and tiny_is_elf
    print('[%s] 3 tiny 闭环：甲言=%r gcc=%r' % ('PASS' if ok3 else 'FAIL', jy_out, gcc_out))
    if not ok3:
        red.append('3 tiny 闭环')

    # 4) 自编译一次
    v1 = os.path.join(work, 'v1')
    rc, out, err = wsl('rm -f %s/v1; "%s" %s -o %s/v1 > /dev/null 2>&1; echo "E=$?"; "%s/v1" --help > /dev/null 2>&1; echo "H=$?"'
                       % (W, wsl_path(elfcc), MAIN_SRC, W, W))
    v1_is_elf = os.path.exists(v1) and io.open(v1, 'rb').read(4) == b'\x7fELF'
    ok4 = ('E=0' in out) and ('H=0' in out) and v1_is_elf
    print('[%s] 4 自编译 v1：%s' % ('PASS' if ok4 else 'FAIL', out.strip().replace('\n', ' ')))
    if not ok4:
        red.append('4 自编译 v1')

    # 5) 固定点 v1 → v2 → v3
    if ok4:
        rc, out, err = wsl('rm -f %s/v2 %s/v3; "%s/v1" %s -o %s/v2 > /dev/null 2>&1; "%s/v2" %s -o %s/v3 > /dev/null 2>&1; '
                           'sha256sum %s/v1 %s/v2 %s/v3 2>/dev/null | cut -d" " -f1'
                           % (W, W, W, MAIN_SRC, W, W, MAIN_SRC, W, W, W, W))
        shas = [l.strip() for l in out.splitlines() if len(l.strip()) == 64]
        ok5 = len(shas) == 3 and len(set(shas)) == 1
        print('[%s] 5 固定点：%s' % ('PASS' if ok5 else 'FAIL', ['%s…' % s[:16] for s in shas]))
        if not ok5:
            red.append('5 固定点')
    else:
        print('[FAIL] 5 固定点（前置 v1 未成）')
        red.append('5 固定点')

    print('=' * 70)
    if red:
        print('ELF 自举判据: FAIL -> %s' % red)
        return 1
    print('ELF 自举判据: PASS（产 ELF 编译器 · --help · tiny 与 gcc 一致 · 自编译 v1 · v1==v2==v3）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
