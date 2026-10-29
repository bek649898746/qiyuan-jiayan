#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_inline280 —— 净缺口 #280「内联完整」的**展开性**判据（2026-10-17 修正版）。

⚠️ 首版判据是假阴性（skeptic 2026-10-17 驳回）:
  只匹配「调用行里含 `加一`/`add2` 字样」，但本编译器发射的是 `调用 .L355`（局部标签），
  未展开时也照样打印 call-to-inline=0 ⟹ 无法区分展开/未展开。

本版判据（走真实 `-S` 产物，用**定义标签**判）:
  ① 从 asm 里找 `加一:` / `add2:` 定义行，取其**前一行**的 `.L<n>:` 标签；
  ② 断言 asm 中 `调用 .L<n>` 出现次数 = 0（展开版）；
  ③ **反向对照**：从锚源码删掉 `内联 `/`inline ` 生成等价「未展开版」，其 asm 必须出现
     `调用 .L<n>` ≥ 1 —— 否则说明判据本身失效（防再次假 PASS）；
  ④ 两个函数名仍作为定义存在。

用法:
  python scripts/check_inline280.py [--cc <编译器 exe>]
"""
import os
import re
import subprocess
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
CC = 'qcc_zh_v2.exe'
if '--cc' in sys.argv:
    CC = sys.argv[sys.argv.index('--cc') + 1]
ANCHOR = os.path.join('scripts', 'feat_zh_inline280_ok.c')
BASE = os.path.join(os.environ.get('TEMP', '.'), '_check_inline280')


def run(args, timeout=180):
    return subprocess.run(args, capture_output=True, timeout=timeout)


def asm_of(src, tag):
    exe = os.path.join(BASE, tag + '.exe')
    r = run([CC, '-S', src, '-o', exe])
    path = exe + '.asm'
    if r.returncode != 0 or not os.path.exists(path):
        return None, (r.stdout + r.stderr).decode('utf-8', 'replace')[:400]
    return open(path, encoding='utf-8', errors='replace').read().splitlines(), ''


def def_label(asm, name):
    """返回 `name:` 定义前一行若为 `.L<n>:` 的标签（即该函数的调用目标）。"""
    for i, l in enumerate(asm):
        if l.strip() == name + ':':
            for j in range(i - 1, max(-1, i - 4), -1):
                m = re.match(r'^(\.L\d+):$', asm[j].strip())
                if m:
                    return m.group(1)
            return None
    return None


def calls_to(asm, label):
    if not label:
        return -1
    n = 0
    for l in asm:
        s = l.strip()
        if s.startswith('调用') and label in s:
            n += 1
    return n


def main():
    os.makedirs(BASE, exist_ok=True)
    # 行为
    exe = os.path.join(BASE, 'inl.exe')
    r = run([CC, ANCHOR, '-o', exe])
    if r.returncode != 0:
        print('[FAIL] compile rc=%d\n%s' % (r.returncode, (r.stdout + r.stderr).decode('utf-8', 'replace')[:600]))
        return 1
    r2 = run([exe])
    out = (r2.stdout + r2.stderr).decode('utf-8', 'replace').strip()
    ok_run = (r2.returncode == 0 and 'INL280 42' in out)
    print('run rc=%d out=%r -> %s' % (r2.returncode, out[:100], 'PASS' if ok_run else 'FAIL'))

    # 展开版 asm
    asm, err = asm_of(ANCHOR, 'exp')
    if asm is None:
        print('[FAIL] -S 展开版:', err)
        return 1
    lab1 = def_label(asm, '加一')
    c1 = calls_to(asm, lab1)
    defs = sum(1 for l in asm if l.strip() == '加一:')
    print('expanded asm: 加一 label=%s calls=%d | defs=%d' % (lab1, c1, defs))

    # 反向对照：删掉 `内联 `/`inline ` 的等价未展开版
    src = open(ANCHOR, encoding='utf-8').read()
    plain = src.replace('内联 函数 ', '函数 ').replace('inline ', '')
    p = os.path.join(BASE, 'noexp.c')
    open(p, 'w', encoding='utf-8').write(plain)
    asm2, err2 = asm_of(p, 'noexp')
    if asm2 is None:
        print('[FAIL] -S 对照版:', err2)
        return 1
    lab1b = def_label(asm2, '加一')
    c1b = calls_to(asm2, lab1b)
    print('control  asm: 加一 label=%s calls=%d' % (lab1b, c1b))

    ok = (ok_run and defs >= 1 and lab1
          and c1 == 0
          and c1b >= 1)
    if not ok:
        if c1b == 0:
            print('  !! 对照版也没有调用 ⟹ 判据本身失效（标签解析错），不是展开证据')
        if c1 != 0:
            print('  !! 展开版仍有调用 ⟹ `内联` 宏未注册 / 未展开')
    print('=== check_inline280: %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
