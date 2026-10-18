# -*- coding: utf-8 -*-
"""净缺口 #120「多态变体」独立判据（2026-10-18）。

**说明**：多态变体在本编译器里以 `判别 名 于 { 变体(载荷), ... };` + `依据 … 穷尽 … 对照 …` 形式**已实现**
（20+ 既有锚）。本判据不新增机制，只把「已实现」变成**可复算的闭环证据**：

  ① 正锚 A `feat_zh_discpayload_ok.c`：类型化载荷 + 依据分派 ⟹ stdout `DP 25`；
  ② 正锚 B `feat_zh_disc_combo_ok.c`：组合面（多实例/数组/字段/嵌套/写回/双载荷/空变体/尺寸）⟹ 期望前缀 `DISC `；
  ③ 负锚 `feat_zh_disc_exhaustive_neg.c`：非穷尽 `依据` ⟹ compile_fail；
  ④ 主源命中：`qcc_zh.jy` 含 `判别` / `直寅` / `依据`。

用法: python scripts/check_variant120.py [--cc <exe>]
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
OUT = os.path.join(REPO, 'build', 'variant120')
os.makedirs(OUT, exist_ok=True)

fails = []


def run_anchor(src, tag, want_prefix):
    exe = os.path.join(OUT, tag + '.exe')
    r = subprocess.run([CC, src, '-o', exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if r.returncode != 0:
        return None, 'compile rc=%d: %s' % (r.returncode, ((r.stdout or '') + (r.stderr or ''))[:200])
    rr = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace')
    out = ((rr.stdout or '') + (rr.stderr or '')).strip()
    return out, None


a, err = run_anchor('scripts/feat_zh_discpayload_ok.c', 'payload', 'DP ')
print('正锚A: out=%r（期望 DP 25）%s' % (a, err or ''))
if err or a != 'DP 25':
    fails.append('正锚A 失败: %r %s' % (a, err or ''))

b, err = run_anchor('scripts/feat_zh_disc_combo_ok.c', 'combo', 'DISC ')
print('正锚B: out=%r（期望 DISC 前缀）%s' % ((b or '')[:80], err or ''))
if err or not (b or '').startswith('DISC '):
    fails.append('正锚B 失败: %r %s' % ((b or '')[:120], err or ''))

rn = subprocess.run([CC, 'scripts/feat_zh_disc_exhaustive_neg.c', '-o', os.path.join(OUT, 'neg.exe')],
                    capture_output=True, text=True, encoding='utf-8', errors='replace')
nt = (rn.stdout or '') + (rn.stderr or '')
print('负锚: rc=%d 含非穷尽=%s' % (rn.returncode, '非穷尽' in nt))
if rn.returncode == 0:
    fails.append('负锚（非穷尽依据）未被拒绝')
elif '非穷尽' not in nt:
    fails.append('负锚未报非穷尽: %r' % (nt[:160],))

src = open('srclib_jiayan/qcc_zh.jy', encoding='utf-8', errors='replace').read()
for tok in ('判别', '直寅', '依据'):
    if tok not in src:
        fails.append('主源缺 `%s`' % tok)

if fails:
    print('=== #120 多态变体判据: FAIL ===')
    for f in fails:
        print('  - ' + f)
    sys.exit(1)
print('=== #120 多态变体判据: PASS（载荷 DP 25 · 组合面 DISC · 非穷尽拒绝 · 主源命中）===')
