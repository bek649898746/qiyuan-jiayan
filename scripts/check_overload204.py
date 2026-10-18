# -*- coding: utf-8 -*-
"""净缺口 #204「方法重载（同名不同参数个数）」独立判据（2026-10-18）。

判据（真入口）：
  ① 正锚 `feat_zh_overload204_ok.c`：同名 `加`（2 参 / 1 参）编译并运行，
     stdout == `OV204 3 6`；
  ② 负锚 `feat_zh_overload204_neg.c`：同名**同参**重复定义必须 compile_fail（A6109）；
  ③ 主源命中：`qcc_zh.jy` 含 `参数数` 字段与 `查函数参` 分派（静态断言）。

用法: python scripts/check_overload204.py [--cc <exe>]
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
OUT = os.path.join(REPO, 'build', 'overload204')
os.makedirs(OUT, exist_ok=True)

fails = []


def build(src, tag):
    exe = os.path.join(OUT, tag + '.exe')
    r = subprocess.run([CC, src, '-o', exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    return r, exe


r, exe = build('scripts/feat_zh_overload204_ok.c', 'ok')
out = ''
if r.returncode != 0:
    fails.append('正锚编译失败 rc=%d: %s' % (r.returncode, ((r.stdout or '') + (r.stderr or ''))[:220]))
else:
    rr = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace')
    out = ((rr.stdout or '') + (rr.stderr or '')).strip()
print('正锚: rc=%d out=%r（期望 OV204 3 6）' % (r.returncode, out))
if out != 'OV204 3 6':
    fails.append('正锚运行值 %r ≠ OV204 3 6' % (out,))

# 正锚 2：**顺序覆盖**（1参先/2参后 · 三重载升序 · 1参先/0参后）—— skeptic 打回后补
r2, exe2 = build('scripts/feat_zh_overload204_order_ok.c', 'order')
out2 = ''
if r2.returncode != 0:
    fails.append('顺序锚编译失败 rc=%d: %s' % (r2.returncode, ((r2.stdout or '') + (r2.stderr or ''))[:220]))
else:
    rr2 = subprocess.run([exe2], capture_output=True, text=True, encoding='utf-8', errors='replace')
    out2 = ((rr2.stdout or '') + (rr2.stderr or '')).strip()
print('顺序锚: rc=%d out=%r（期望 ORD 6 3 15 13 16 200）' % (r2.returncode, out2))
if out2 != 'ORD 6 3 15 13 16 200':
    fails.append('顺序锚运行值 %r ≠ ORD 6 3 15 13 16 200' % (out2,))

rn, _ = build('scripts/feat_zh_overload204_neg.c', 'neg')
print('负锚: rc=%d（期望 != 0，A6109）' % rn.returncode)
if rn.returncode == 0:
    fails.append('负锚（同名同参重复）未被拒绝')

src = open('srclib_jiayan/qcc_zh.jy', encoding='utf-8', errors='replace').read()
for tok in ('参数数', '查函数参'):
    if tok not in src:
        fails.append('主源缺 `%s`（重载实现未落）' % tok)

if fails:
    print('=== #204 重载判据: FAIL ===')
    for f in fails:
        print('  - ' + f)
    sys.exit(1)
print('=== #204 重载判据: PASS（OV204 3 6 · 同参重复拒绝 · 主源命中）===')
