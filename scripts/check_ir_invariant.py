# -*- coding: utf-8 -*-
"""check_ir_invariant.py —— **IR 不变量 破坏点名** 检查器（plan 判据 3 后半: 「人为破坏 ⟹ 报出 哪一条、在哪」）。

背景（为什么需要它）:
  `不变构跑()` 已落 6 条容量/越界不变量（A1/A2/A3/A5/F3/F4），违反时 `不变违++` 并 打
  `[ERR] IR-INVARIANT <名>: <上下文值>`；且 已有 负向打靶 开关 `QCC_DBG_INV_BREAK=1`
  （**反转 A2 判据** ⟹ 正常程序 也 必违 ⟹ 证明 检查器 真在工作）。
  ⟹ 但 **缺一个 判据器** 把这一能力 变成 **可复算的 rc**：本器 补上。

判据（三条, 全过 ⟹ rc=0）:
  ① **正常组**（不设开关）: dump 含 `**不变构**: 不变违=N` 且 **N == 0**（无违反）
  ② **正常组 无违例行**: dump **不含** `[ERR] IR-INVARIANT`（否则 正常编译 就有 真违例）
  ③ **破坏组**（QCC_DBG_INV_BREAK=1）: dump 含 `[ERR] IR-INVARIANT`，且 **点名 不变量代号**
     （形如 `A2(负向打靶)`）+ **在哪**（同一行 带 具体容量对 `<名>(%d) == <名>(%d)`）⟹ 不是 空转

用法: python scripts/check_ir_invariant.py [编译器]
退出码: 0 = 三条 判据 全过; 1 = 有判据 不成立
"""
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(QJ)
TMP = os.path.join(os.environ.get('TEMP', '.'), '_ir_invariant')
os.makedirs(TMP, exist_ok=True)


def find_qcc():
    env = os.environ.get('QCC_TEST_BIN')
    if env and os.path.exists(env):
        return env
    for n in range(7, 0, -1):
        p = os.path.join(QJ, 'qcc_zh_v%d.exe' % n)
        if os.path.exists(p):
            return p
    return os.path.join(QJ, 'qcc_zh_seed.exe')


CC = sys.argv[1] if len(sys.argv) > 1 else find_qcc()
SRC = os.path.join(QJ, 'scripts', 'probe', 'ir_cover_probe.c')
RE_INV = re.compile(r'\*\*不变构\*\*:\s*不变违=(-?\d+)')
RE_BREAK = re.compile(r'\[ERR\] IR-INVARIANT ([A-Z]+\d+)\(负向打靶\):\s*(\S+?)\((-?\d+)\)\s*==\s*(\S+?)\((-?\d+)\)')


def dump(tag, extra_env, dump_ir=True):
    exe = os.path.join(TMP, 'inv_%s.exe' % tag)
    env = dict(os.environ)
    env.update(extra_env)
    if dump_ir:
        env['QCC_DUMP'] = 'ir'
    r = subprocess.run([CC, SRC, '-o', exe], capture_output=True, cwd=QJ, env=env)
    # 实测: dump 走 **stdout**（stderr 为空）⟹ 两侧合流后一起查
    return ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace'), r.returncode


fails = []
print('[check_ir_invariant] 编译器 = %s' % CC)

# ---- ① / ② 正常组 ----
t0, rc0 = dump('normal', {})
m0 = RE_INV.search(t0)
if not m0:
    fails.append('正常组: 未见表 「**不变构**: 不变违=N」段')
    print('  ① 正常组: ✗ 未见 不变构 段')
else:
    n0 = int(m0.group(1))
    ok1 = (n0 == 0)
    print('  ① 正常组: 不变违=%d ⟹ 不变量 全过 %s' % (n0, '✓' if ok1 else '✗'))
    if not ok1:
        fails.append('正常组: 不变违=%d (应 0)' % n0)

viol = re.findall(r'\[ERR\] IR-INVARIANT [^:]+: [^\n]*', t0)
ok2 = (len(viol) == 0)
print('  ② 正常组 无违例行: %s' % ('✓' if ok2 else '✗ %d 行' % len(viol)))
if not ok2:
    fails.append('正常组: 出现 %d 条 IR-INVARIANT 违例行（正常编译不该有）' % len(viol))

# ---- ③ 破坏组 (负向打靶) ----
t1, rc1 = dump('break', {'QCC_DBG_INV_BREAK': '1'})
mb = RE_BREAK.search(t1)
m1 = RE_INV.search(t1)
if not mb:
    lines = [l for l in t1.split('\n') if 'IR-INVARIANT' in l]
    print('  ③ 破坏组: ✗ 未点名（IR-INVARIANT 行 = %d）' % len(lines))
    fails.append('破坏组: 未 报出 点名行 [ERR] IR-INVARIANT <代号>(负向打靶): …')
else:
    name, obj1, v1, obj2, v2 = mb.groups()
    print('  ③ 破坏组: 点名 %s ⟹ %s(%s) == %s(%s)  ✓'
          % (name, obj1, v1, obj2, v2))
    if name != 'A2':
        fails.append('破坏组: 点名 代号=%s (预期 A2)' % name)
    if m1 and int(m1.group(1)) <= 0:
        fails.append('破坏组: 不变违=%s (破坏后 应 > 0)' % m1.group(1))
    else:
        print('     不变违=%s  ⟹ 计数 同步 ✓' % (m1.group(1) if m1 else '?'))

print()
if fails:
    print('[FAIL] 不变量 破坏点名 判据 不成立:')
    for f in fails:
        print('   -', f)
    sys.exit(1)
print('[PASS] 不变量 破坏点名: 正常组 不变违=0 且 无违例行 · 破坏组 点名 代号(A2) + 在哪(容量对) ⟹ 非空转')
sys.exit(0)
