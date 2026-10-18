# -*- coding: utf-8 -*-
"""check_ir_hash64.py —— 逐遍快照哈希 **64 位口径** 判据器（清单 T6-1）。

清单原文验收：「跑一 pass ⟹ 出现 **64 位**哈希；**同 IR 重跑哈希相同**；**改一算子哈希必变**」。

判据（驱动**真实终态引擎**，不重实现）:
  ① **64 位口径成对**: dump 出现 `**照散64**: 前 高=H 低=L · 后 高=H2 低=L2`（四分量齐备，且高段不恒 0）
  ② **同 IR 重跑同哈希**: 同源两次独立编译 ⟹ 四分量逐字相同
  ③ **改一算子必变**: 源内一个常量改动（3+4 → 3+5）⟹ 至少一分量变
  ④ **本趟真变换**: 前/后两组不相同（pass 真改了 L1 影子）
退出码: 0 = 四条全过
"""
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
TMP = os.path.join(os.environ.get('TEMP', '.'), '_ir_hash64')
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
SRC_A = os.path.join(QJ, 'scripts', 'probe', 'ir_fold_probe.c')

txt_a = open(SRC_A, encoding='utf-8-sig').read()
txt_b = txt_a.replace('3 + 4', '3 + 5', 1)
if txt_a == txt_b:
    print('[FAIL] 变体生成失败：源内未找到 `3 + 4`')
    sys.exit(1)
SRC_B = os.path.join(TMP, 'ir_fold_probe_plus1.c')
with open(SRC_B, 'w', encoding='utf-8') as f:
    f.write(txt_b)

RE64 = re.compile(r'\*\*照散64\*\*:\s*前 高=(-?\d+) 低=(-?\d+) · 后 高=(-?\d+) 低=(-?\d+)')


def run(src, tag):
    exe = os.path.join(TMP, 'h64_%s.exe' % tag)
    r = subprocess.run([CC, src, '-o', exe], capture_output=True, cwd=QJ,
                       env=dict(os.environ, QCC_DUMP='ir'))
    return ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')


fails = []
print('[check_ir_hash64] 编译器 = %s' % CC)

t1, t2, tb = run(SRC_A, 'a1'), run(SRC_A, 'a2'), run(SRC_B, 'b')
m1, m2, mb = RE64.search(t1), RE64.search(t2), RE64.search(tb)

if not (m1 and m2 and mb):
    print('  ✗ 未见 `**照散64**` 通道行（64 位口径 未落位）')
    for lbl, t in (('A1', t1), ('B', tb)):
        print('     %s: %s' % (lbl, [l for l in t.split('\n') if '照散' in l][:2]))
    sys.exit(1)

a1 = tuple(m1.groups())
a2 = tuple(m2.groups())
b1 = tuple(mb.groups())

ok1 = (int(a1[0]) != 0 or int(a1[2]) != 0)   # 高段 不恒 0 ⟹ 真 64 位 口径
print('  ① 64 位口径成对: 前三高=%s 后 高=%s ⟹ %s' % (a1[0], a1[2], '✓' if ok1 else '✗ 高段恒 0'))
if not ok1:
    fails.append('高段恒 0 ⟹ 非真 64 位口径')

ok2 = (a1 == a2)
print('  ② 同源重跑同哈希: A1=%s A2=%s ⟹ %s' % (a1, a2, '✓' if ok2 else '✗'))
if not ok2:
    fails.append('同源两次编译 64 位分量不同')

ok3 = (b1 != a1)
print('  ③ 改一算子必变: B=%s vs A=%s ⟹ %s' % (b1, a1, '✓ 必变' if ok3 else '✗ 未变'))
if not ok3:
    fails.append('改一算子后 64 位分量未变')

ok4 = (a1[0], a1[1]) != (a1[2], a1[3])
print('  ④ 本趟真变换: 前=(%s,%s) 后=(%s,%s) ⟹ %s' % (a1[0], a1[1], a1[2], a1[3], '✓' if ok4 else '✗'))
if not ok4:
    fails.append('前/后 相同 ⟹ pass 未生效')

print()
if fails:
    print('[FAIL] T6-1 64 位哈希 判据 不成立:')
    for f in fails:
        print('   -', f)
    sys.exit(1)
print('[PASS] T6-1 64 位逐遍快照哈希: 口径成对（高段非 0）· 同源同哈希 · 改一算子必变 · 本趟真变换')
sys.exit(0)
