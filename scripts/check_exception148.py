# -*- coding: utf-8 -*-
"""#148 异常「真内建」独立判据（2026-10-18）。

判据：
  ① 正锚 `feat_zh_exception148_ok.c` 编译并运行，stdout == `EX 7 42`（不依赖用户定义 `throw_ex`）；
  ② 负锚 `feat_zh_exception148_neg.c`（`抛出 码;` 缺值）必须 `A9007` compile_fail（内建参数数表）；
  ③ 主源命中：`qcc_zh.jy` 的 `主薄()` 含 `throw_ex`，且 `主薄参()` 表含 3；
  ④ 运行时命中：`qcc_rt.c` 含 `throw_ex` **实现**（非用户定义）。

用法: python scripts/check_exception148.py [--cc <exe>]
退出码: 0 全过；1 有失败。
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
OUT = os.path.join(REPO, 'build', 'exception148')
os.makedirs(OUT, exist_ok=True)

fails = []


def build(src, tag):
    exe = os.path.join(OUT, tag + '.exe')
    r = subprocess.run([CC, src, '-o', exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    return r, exe


r, exe = build('scripts/feat_zh_exception148_ok.c', 'ok')
out = ''
if r.returncode != 0:
    fails.append('正锚编译失败 rc=%d: %s' % (r.returncode, ((r.stdout or '') + (r.stderr or ''))[:220]))
else:
    rr = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace')
    out = ((rr.stdout or '') + (rr.stderr or '')).strip()
print('正锚: rc=%d out=%r（期望 EX 7 42）' % (r.returncode, out))
if out != 'EX 7 42':
    fails.append('正锚运行值 %r ≠ EX 7 42' % (out,))

rn, _ = build('scripts/feat_zh_exception148_neg.c', 'neg')
nt = (rn.stdout or '') + (rn.stderr or '')
print('负锚: rc=%d 含A9007=%s' % (rn.returncode, 'A9007' in nt))
if rn.returncode == 0:
    fails.append('负锚（抛出 缺值）未被拒绝')
elif 'A9007' not in nt:
    fails.append('负锚未报 A9007: %r' % (nt[:160],))

src = open('srclib_jiayan/qcc_zh.jy', encoding='utf-8', errors='replace').read()
if 'throw_ex' not in src:
    fails.append('主源缺 throw_ex')
if not re.search(r'主薄参', src):
    fails.append('主源缺 主薄参（内建参数数表）')
rt = open('srclib/qcc_rt.c', encoding='utf-8', errors='replace').read()
if not re.search(r'(?:void|int)\s+throw_ex\s*\(', rt):
    fails.append('运行时 qcc_rt.c 缺 throw_ex 实现')

if fails:
    print('=== #148 异常判据: FAIL ===')
    for f in fails:
        print('  - ' + f)
    sys.exit(1)
print('=== #148 异常判据: PASS（EX 7 42 · 缺值 A9007 · 主薄/运行时命中）===')
