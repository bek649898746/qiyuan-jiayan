# -*- coding: utf-8 -*-
"""净缺口 #63「数据并行」独立判据（2026-10-18）。

判据（真入口）：
  ① 正锚 `feat_zh_simd63_ok.c` 编译并运行，stdout == `SIMD 11 22 33 44`；
  ② 负锚 `feat_zh_simd63_neg.c`（`向量加` 缺参）必须 A9007 compile_fail；
  ③ 主源命中：`qcc_zh.jy` 含 `向量加`；运行时 `qcc_rt.c` 含 `向量加` 实现。

用法: python scripts/check_simd63.py [--cc <exe>]
退出码: 0 全过；1 有失败。
"""
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
CC = 'qcc_zh_v2.exe'
if '--cc' in sys.argv:
    CC = sys.argv[sys.argv.index('--cc') + 1]
OUT = os.path.join(REPO, 'build', 'simd63')
os.makedirs(OUT, exist_ok=True)

fails = []


def build(src, tag):
    exe = os.path.join(OUT, tag + '.exe')
    r = subprocess.run([CC, src, '-o', exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    return r, exe


r, exe = build('scripts/feat_zh_simd63_ok.c', 'ok')
out = ''
if r.returncode != 0:
    fails.append('正锚编译失败 rc=%d: %s' % (r.returncode, ((r.stdout or '') + (r.stderr or ''))[:220]))
else:
    rr = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace')
    out = ((rr.stdout or '') + (rr.stderr or '')).strip()
print('正锚: rc=%d out=%r（期望 SIMD 11 22 33 44）' % (r.returncode, out))
if out != 'SIMD 11 22 33 44':
    fails.append('正锚运行值 %r ≠ SIMD 11 22 33 44' % (out,))

# ②b `-c` 模式正探针（灵敏度：旧 exe 未登记 `向量加` ⟹ A1001；新 exe ⟹ 外部符号）
obj = os.path.join(OUT, 'ok.obj')
rc_c = subprocess.run([CC, '-c', 'scripts/feat_zh_simd63_ok.c', '-o', obj],
                      capture_output=True, text=True, encoding='utf-8', errors='replace')
nm = ''
if rc_c.returncode == 0 and os.path.exists(obj):
    nr = subprocess.run(['nm', obj], capture_output=True, text=True, encoding='utf-8', errors='replace')
    nm = nr.stdout or ''
print('-c 模式: rc=%d 含U向量加=%s' % (rc_c.returncode, 'U 向量加' in nm))
if rc_c.returncode != 0:
    fails.append('-c 正探针失败（内建名未登记）: %s' % (((rc_c.stdout or '') + (rc_c.stderr or ''))[:160]))
elif 'U 向量加' not in nm:
    fails.append('-c 目标文件缺 U 向量加 外部引用')

rn, _ = build('scripts/feat_zh_simd63_neg.c', 'neg')
nt = (rn.stdout or '') + (rn.stderr or '')
print('负锚: rc=%d 含A9007=%s' % (rn.returncode, 'A9007' in nt))
if rn.returncode == 0:
    fails.append('负锚（向量加 缺参）未被拒绝')
elif 'A9007' not in nt:
    fails.append('负锚未报 A9007: %r' % (nt[:160],))

src = open('srclib_jiayan/qcc_zh.jy', encoding='utf-8', errors='replace').read()
if '向量加' not in src:
    fails.append('主源缺 `向量加`')
rt = open('srclib/qcc_rt.c', encoding='utf-8', errors='replace').read()
if '向量加' not in rt:
    fails.append('运行时 qcc_rt.c 缺 `向量加` 实现')

if fails:
    print('=== #63 数据并行判据: FAIL ===')
    for f in fails:
        print('  - ' + f)
    sys.exit(1)
print('=== #63 数据并行判据: PASS（SIMD 11 22 33 44 · 缺参 A9007 · 主源/运行时命中）===')
