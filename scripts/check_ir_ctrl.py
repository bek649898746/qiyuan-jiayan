# -*- coding: utf-8 -*-
"""R715Q 刀四 判据检查器：`抽跳`1041 / `抽条件跳`1042 / `抽选择`1111 是否 真发射 + 计数自洽 + 标号零发射。
判据:
  ① 三类控制流码 各 ≥1（语料含 若/循环/遍/断/续/跳/标号/三元）
  ② **goto 的 源一 == 标签 id ≠ 0**（目标 可解析 ⟹ 与 刀三 同一约定）
  ③ **标号节点 零发射**：配对语料（仅差 `跳 尾;`+`尾:`）⟹ 控制流码 总数 差 **恰好 +1**
  ④ 控制流码 总数 ≤ `控式!=0` 节点数（上界 自洽）
"""
import os, re, subprocess, sys
from collections import Counter

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
T = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'probe')  # R821e: 探针 随仓走（原 只读 %TEMP%/qiyuan-ir-knives ⇹ 目录被清理后 假红）
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(QJ, 'qcc_zh_v2.exe')
CTRL = {1041: '抽跳', 1042: '抽条件跳', 1111: '抽选择'}
fails = []


def run(name):
    src = os.path.join(T, name + '.c')
    if not os.path.exists(src):
        return None
    r = subprocess.run([CC, src, '-o', os.path.join(T, name + '.exe')],
                       capture_output=True, env=dict(os.environ, QCC_DUMP='ir'), cwd=QJ)
    o = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    l1 = re.findall(r'抽\[(\d+)\] 算子=(\d+) 现高=\d+ 元=(-?\d+) 源一=(-?\d+) 源二=(-?\d+) 链头=', o)
    l1 = [(int(a), int(b), int(c), int(d), int(e)) for a, b, c, d, e in l1]
    kong = [int(c) for _, c, _ in re.findall(r'节\[(\d+)\] 控式=(\d+) 段双=(\d+)', o)]
    runout = ''
    exe = os.path.join(T, name + '.exe')
    if r.returncode == 0 and os.path.exists(exe):
        rr = subprocess.run([exe], capture_output=True)
        runout = (rr.stdout or b'').decode('utf-8', 'replace').strip()
    return r.returncode, l1, kong, runout


print('编译器 =', CC)
res = {}
for nm in ['kw3_h', 'kw3_b', 'kw3_g', 'kw3_e', 'kw3_f']:
    got = run(nm)
    if got is None:
        print('%-8s 缺件' % nm); continue
    rc, l1, kong, runout = got
    c = Counter(x[1] for x in l1)
    ctrl_n = sum(v for k, v in c.items() if k in CTRL)
    marked = sum(1 for k in kong if k != 0)
    res[nm] = (ctrl_n, marked, l1)
    print('%-8s rc=%d  L1条数=%-5d 控制流码=%d (跳%d/条件跳%d/选择%d) 控式!=0 节数=%-4d 运行=%r' % (
        nm, rc, len(l1), ctrl_n, c.get(1041, 0), c.get(1042, 0), c.get(1111, 0), marked, runout))

# ① 三类各 ≥1（用 kw3_h + kw3_f）
h = res.get('kw3_h'); f = res.get('kw3_f')
allc = Counter()
for nm in ('kw3_h', 'kw3_f'):
    if nm in res:
        allc += Counter(x[1] for x in res[nm][2] if x[1] in CTRL)
ok1 = all(allc.get(k, 0) >= 1 for k in CTRL)
print('\n① 三类控制流码 各 ≥1: %s  %s' % ('✓' if ok1 else '✗', {CTRL[k]: allc.get(k, 0) for k in CTRL}))
if not ok1: fails.append('判据①')

# ② goto 源一 == 标签 id ≠ 0
gate = [x for x in (f[2] if f else []) if x[1] == 1041]
ok2 = any(x[3] != 0 for x in gate)
print('② goto 的 源一(=标签id) ≠ 0: %s  %s' % ('✓' if ok2 else '✗', [(x[0], x[3]) for x in gate if x[3] != 0][:3]))
if not ok2: fails.append('判据②')

# ③ 配对：kw3_b(有 跳+标号) vs kw3_g(无) ⟹ 差 恰 +1
if 'kw3_b' in res and 'kw3_g' in res:
    d = res['kw3_b'][0] - res['kw3_g'][0]
    ok3 = (d == 1)
    print('③ 标号零发射（配对差 = %+d，期望 +1）: %s' % (d, '✓' if ok3 else '✗'))
    if not ok3: fails.append('判据③')
else:
    print('③ 配对缺件'); fails.append('判据③缺件')

# ④ 上界
ok4 = all(v[0] <= v[1] for v in res.values())
print('④ 控制流码总数 ≤ 控式!=0 节数: %s' % ('✓' if ok4 else '✗'))
if not ok4: fails.append('判据④')

print('\n[%s] R715Q 刀四 判据 %s' % ('PASS' if not fails else 'FAIL', '' if not fails else fails))
sys.exit(0 if not fails else 1)
