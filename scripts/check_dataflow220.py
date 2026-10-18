# -*- coding: utf-8 -*-
"""净缺口 #220「声明式数据流（依赖图）」独立判据（2026-10-18）。

判据（真入口，依赖解析期 AST 而非文本扫描）：
  ① 正锚 `feat_zh_dataflow220_ok.c` 编译并运行，stdout == `DF 6`；
  ② `QCC_DUMP` 非空时，编译器输出含 `数据流图: 甲->乙`（真依赖边）；
  ③ 负锚 `feat_zh_dataflow220_neg.c`（自环 `甲 = 甲 + 1`）必须 `A9011` compile_fail；
  ④ 主源命中：`qcc_zh.jy` 含 `数据流扫` 与 `A9011`。

用法: python scripts/check_dataflow220.py [--cc <exe>]
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
OUT = os.path.join(REPO, 'build', 'dataflow220')
os.makedirs(OUT, exist_ok=True)

fails = []


def build(src, tag, env=None):
    exe = os.path.join(OUT, tag + '.exe')
    r = subprocess.run([CC, src, '-o', exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', env=env)
    return r, exe


r, exe = build('scripts/feat_zh_dataflow220_ok.c', 'ok')
out = ''
if r.returncode != 0:
    fails.append('正锚编译失败 rc=%d: %s' % (r.returncode, ((r.stdout or '') + (r.stderr or ''))[:220]))
else:
    rr = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace')
    out = ((rr.stdout or '') + (rr.stderr or '')).strip()
print('正锚: rc=%d out=%r（期望 DF 6）' % (r.returncode, out))
if out != 'DF 6':
    fails.append('正锚运行值 %r ≠ DF 6' % (out,))

env = dict(os.environ); env['QCC_DUMP'] = 'ir'
rd, _ = build('scripts/feat_zh_dataflow220_ok.c', 'dump', env)
dt = (rd.stdout or '') + (rd.stderr or '')
hit = '数据流图:' in dt and '甲->乙' in dt.replace(' ', '')
print('依赖图 dump: rc=%d 命中=%s' % (rd.returncode, hit))
if not hit:
    fails.append('依赖图 dump 未含 甲->乙: %r' % (dt[:200],))

rn, _ = build('scripts/feat_zh_dataflow220_neg.c', 'neg')
nt = (rn.stdout or '') + (rn.stderr or '')
print('负锚: rc=%d 含A9011=%s' % (rn.returncode, 'A9011' in nt))
if rn.returncode == 0:
    fails.append('负锚（自环）未被拒绝')
elif 'A9011' not in nt:
    fails.append('负锚未报 A9011: %r' % (nt[:160],))

src = open('srclib_jiayan/qcc_zh.jy', encoding='utf-8', errors='replace').read()
for tok in ('数据流扫', 'A9011'):
    if tok not in src:
        fails.append('主源缺 `%s`' % tok)

if fails:
    print('=== #220 数据流判据: FAIL ===')
    for f in fails:
        print('  - ' + f)
    sys.exit(1)
print('=== #220 数据流判据: PASS（DF 6 · 依赖图 甲->乙 · 自环 A9011）===')
