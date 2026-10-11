# -*- coding: utf-8 -*-
"""R716 检查器（刀七-A + 刀八-B）：
   ① 完整性表 **零空白**：读 `R716A_L0对应L1对照表.md` 的表 A/表 B 逐行 ⟹ 每行 必须
      「有码」或「`无` + 理由（同一行内有解释文字）」⟹ **独立复算 未定性 = 0**
   ② **读变量 有码 且 有发射点**：
      · 静态：源码 `例 节点_变量:` 之后 有 `发抽象(数, 抽载,` ；且 `抽载` 有 `#define`
      · 动态（对照）：两份源 只差 变量读 次数 ⟹ dump 里 `抽载` 条数 差 **恰 = 读次数差**
   ③ **def-use 交叉一致**：dump 里 `IR def-use 链` 行 的 `违例=0`（⟹ 成立）
   ④ **def-use 真实写入点**：`发抽象` 函数体内 有 `记用途(源一, 节号); 记用途(源二, 节号);`
"""
import io, os, re, subprocess, sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
S = r'C:\Users\ADMINI~1\AppData\Local\Temp\grok-goal-bc44933af5ae\implementer'
os.makedirs(S, exist_ok=True)
TBL = os.path.join(QJ, '_audit_inbox', '20261011_虾米回执_IR层落位_虾米', 'R716A_L0对应L1对照表.md')
SRC = os.path.join(QJ, 'srclib_jiayan', 'qcc_zh.jy')
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(QJ, 'qcc_zh_v2.exe')
fails = []

# ---------- ① 完整性表 零空白 ----------
md = io.open(TBL, encoding='utf-8', errors='replace').read()
rows = [l for l in md.split('\n') if re.match(r'^\|\s*\d+\s*\|', l)]
A = [l for l in rows if '`' in l or '无' in l]
und = []
for l in A:
    cells = [c.strip() for c in l.strip().strip('|').split('|')]
    if len(cells) < 3:
        continue
    l1 = cells[-1]          # ★ 末列 = `→ L1` 列（表 A 有 4 列: 号 | L0 op | 语义 | → L1）
    if l1 == '':
        und.append(l)
    elif '无' in l1 and len(re.sub(r'[\s`*·、—\-]', '', l1.replace('无', ''))) == 0:
        und.append(l)   # 只有 "无" 没有任何理由 ⟹ 未定性
print('① 完整性表：表 A 行数 = %d' % len(A))
print('   未定性（既无码也无理由）= %d   %s' % (len(und), '✓' if not und else '✗'))
for u in und[:3]:
    print('     ✗', u[:120])
if und:
    fails.append('判据①')

# 表 A 的三类计数（独立复算）
cnt = {'有码': 0, '无理由': 0}
for l in A:
    cells = [c.strip() for c in l.strip().strip('|').split('|')]
    if len(cells) < 3:
        continue
    l1 = cells[-1]
    if '`抽' in l1:
        cnt['有码'] += 1
    elif '无' in l1:
        cnt['无理由'] += 1
print('   复算：有码 %d · 无对应+理由 %d · 合计 %d' % (cnt['有码'], cnt['无理由'], cnt['有码'] + cnt['无理由']))
# ★ 硬断言: 三类必须 铺满 全部行（否则就是"有我说不清的行" = 隐患）
if cnt['有码'] + cnt['无理由'] != len(A):
    print('   ✗ 有 %d 行 两类都不落 ⟹ 逐行列出（不许有说不清的行）:' % (len(A) - cnt['有码'] - cnt['无理由']))
    for l in A:
        cells = [c.strip() for c in l.strip().strip('|').split('|')]
        if len(cells) < 3:
            print('      [列不足]', l[:110]); continue
        l1 = cells[-1]
        if '`抽' not in l1 and '无' not in l1:
            print('      [说不清]', l[:130])
    fails.append('判据①-铺满')

# ---------- ② 读变量 ----------
src = io.open(SRC, encoding='utf-8-sig', errors='replace').read()
has_code = bool(re.search(r'#define\s+抽载\s+\d+', src))
has_emit = '例 节点_变量: { ' in src and '发抽象(数, 抽载, 数, -1, 0);' in src
print('② 读变量：`抽载` 有 #define = %s · `例 节点_变量` 有发射点 = %s %s' % (
    has_code, has_emit, '✓' if (has_code and has_emit) else '✗'))
if not (has_code and has_emit):
    fails.append('判据②-静态')

A_SRC = '#包含 <stdio.h>\n函数 整 主(空) { 整 甲 = 1; 整 乙 = 2; 返 0; }\n'
B_SRC = '#包含 <stdio.h>\n函数 整 主(空) { 整 甲 = 1; 整 乙 = 2; 返 甲 - 甲 - 乙 + 乙; }\n'
env = dict(os.environ, QCC_DUMP='ir')


def count_zai(text):
    return len(re.findall(r'抽\[\d+\] 算子=1051 ', text))


res = {}
for tag, s in (('A(0 次读)', A_SRC), ('B(4 次读)', B_SRC)):
    p = os.path.join(S, 'rv_%s.c' % tag[0])
    io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
    r = subprocess.run([CC, p, '-o', os.path.join(S, 'rv_%s.exe' % tag[0])],
                       capture_output=True, env=env, cwd=QJ)
    o = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    res[tag] = (r.returncode, count_zai(o))
    print('     %-12s rc=%d  抽载(1051) 条数 = %d' % (tag, r.returncode, res[tag][1]))
d = res['B(4 次读)'][1] - res['A(0 次读)'][1]
ok = (d == 4)
print('     对照差 = %+d（期望 +4 = 4 次变量读）  %s' % (d, '✓' if ok else '✗'))
if not ok:
    fails.append('判据②-动态')

# ---------- ③④ def-use ----------
print('③ def-use 交叉一致：', end=' ')
r = subprocess.run([CC, os.path.join(S, 'rv_B.c'), '-o', os.path.join(S, 'rv_B2.exe')],
                   capture_output=True, env=env, cwd=QJ)
o = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
m = re.search(r'IR def-use 链 .*?用数=(\d+); 交叉一致: 应查=(\d+) 违例=(\d+) ⟹ (\S+)\)', o)
if not m:
    print('✗ 未见 def-use 段'); fails.append('判据③')
else:
    use, chk, vio, verdict = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4)
    ok3 = (vio == 0 and chk > 0)
    print('%s（用数=%d 应查=%d 违例=%d）%s' % (verdict, use, chk, vio, '✓' if ok3 else '✗'))
    if not ok3:
        fails.append('判据③')
ok4 = '记用途(源一, 节号); 记用途(源二, 节号);' in src
print('④ def-use 写入点 在 唯一写口 `发抽象` 内：%s' % ('✓' if ok4 else '✗'))
if not ok4:
    fails.append('判据④')

print()
print('[%s] R716 检查器 %s' % ('PASS' if not fails else 'FAIL', '' if not fails else fails))
sys.exit(0 if not fails else 1)
