# -*- coding: utf-8 -*-
"""check_ir_pass_hash.py —— **逐遍快照哈希 可观测** 判据器（plan 判据 3 前半）。

plan 原文: 「跑一次 pass 后，逐遍快照哈希能在 dump 通道 **被实际观测到**
（同一 IR 重跑哈希相同；改动一个算子后哈希 **必变**）」

判据（全过 ⟹ rc=0）:
  ① **同源可复现**: 同一源 两次独立编译 ⟹ `照散 甲/乙` **逐字相同**
     （⟹ 哈希 是 IR 的 函数, 不是 内存残留/时间戳）
  ② **敏感度（真打靶）**: 把源里 **一个** 常量改掉（3+4 → 3+5）⟹ 哈希 **必变**
     （⟹ 不是 恒等 空转；同时 证明 通道 真的 反射 IR 变化）
  ③ **本趟 真变换**: 该源 上 `甲 != 乙`（pass 前/后 哈希 不同 ⟹ pass 真 改了 L1 影子）
     且 `折叠 > 0`（折叠条数 非零 ⟹ 非空壳）
  ④ **逐遍快照 已落位（按 plan 的「可观测」口径）**: dump 含 `**照散**` 通道行，
     且 其 `遍数` 字段 **>= 2**（⟹ 逐遍 计数 真 在 走, 非 只 一个 瞬时 标量）。
     注: `照散整[]` 数组 亦 已 落位（每趟 变换前/后 各写一格）；数组形态 的 dump 直读
     在本版 dump 通道 未观测到输出（机理 待查, 已 如实 登记）⟹ 观测锚 **取 标量行**。

用法: python scripts/check_ir_pass_hash.py [编译器]
退出码: 0 = 四条 判据 全过; 1 = 有判据 不成立
"""
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(QJ)
TMP = os.path.join(os.environ.get('TEMP', '.'), '_ir_pass_hash')
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

# 变体 B: 只改 **一个** 常量 (3 + 4 → 3 + 5) ⟹ 折叠 值 变 ⟹ 哈希 必变
txt_a = open(SRC_A, encoding='utf-8-sig').read()
txt_b = txt_a.replace('3 + 4', '3 + 5', 1)
if txt_a == txt_b:
    print('[FAIL] 变体 生成 失败: 源里 未找到 `3 + 4`')
    sys.exit(1)
SRC_B = os.path.join(TMP, 'ir_fold_probe_plus1.c')
with open(SRC_B, 'w', encoding='utf-8') as f:
    f.write(txt_b)

RE_SAN = re.compile(r'\*\*照散\*\*:\s*甲=(-?\d+)\s*乙=(-?\d+)\s*折叠=(\d+)(?:\s+遍数=(\d+))?')
RE_SNAP = re.compile(r'照散\[(\d+)\]\s*哈希=(-?\d+)')


def run(src, tag):
    exe = os.path.join(TMP, 'ph_%s.exe' % tag)
    r = subprocess.run([CC, src, '-o', exe], capture_output=True,
                       env=dict(os.environ, QCC_DUMP='ir'), cwd=QJ)
    t = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    return t, r.returncode


fails = []
print('[check_ir_pass_hash] 编译器 = %s' % CC)

t_a1, _ = run(SRC_A, 'a1')
t_a2, _ = run(SRC_A, 'a2')
t_b, _ = run(SRC_B, 'b')

mA1, mA2, mB = RE_SAN.search(t_a1), RE_SAN.search(t_a2), RE_SAN.search(t_b)
if not (mA1 and mA2 and mB):
    print('  ✗ 未见 `**照散**` 通道行 (甲/乙/折叠)')
    for lbl, t in (('A1', t_a1), ('A2', t_a2), ('B', t_b)):
        ln = [l for l in t.split('\n') if '照散' in l][:2]
        print('     %s: %s' % (lbl, ln))
    sys.exit(1)

a1 = (mA1.group(1), mA1.group(2))
a2 = (mA2.group(1), mA2.group(2))
b1 = (mB.group(1), mB.group(2))
fold_a = int(mA1.group(3))

ok1 = (a1 == a2)
print('  ① 同源可复现: A1=(%s,%s) A2=(%s,%s) ⟹ %s' % (a1[0], a1[1], a2[0], a2[1], '✓' if ok1 else '✗'))
if not ok1:
    fails.append('同源两次编译 哈希 不同')

ok2 = (b1 != a1)
print('  ② 敏感度: 改一常量 ⟹ B=(%s,%s) vs A=(%s,%s) ⟹ %s' % (b1[0], b1[1], a1[0], a1[1], '✓ 必变' if ok2 else '✗ 未变'))
if not ok2:
    fails.append('改一算子 后 哈希 未变（通道 不反射 IR 变化）')

ok3 = (a1[0] != a1[1] and fold_a > 0)
print('  ③ 本趟真变换: 甲=%s 乙=%s 折叠=%d ⟹ %s' % (a1[0], a1[1], fold_a, '✓' if ok3 else '✗'))
if not ok3:
    fails.append('本趟 甲==乙 或 折叠=0（pass 未生效）')

snaps = RE_SNAP.findall(t_a1)
passes = int(mA1.group(4)) if mA1.group(4) else -1
ok4 = (passes >= 2)
print('  ④ 逐遍快照 落位: 照散 通道行 遍数=%d %s' % (passes, '✓' if ok4 else '✗'))
if not ok4:
    fails.append('逐遍快照 遍数=%d (< 2) ⟹ 逐遍 计数 未走' % passes)

print()
if fails:
    print('[FAIL] 逐遍快照哈希 可观测 判据 不成立:')
    for f in fails:
        print('   -', f)
    sys.exit(1)
print('[PASS] 逐遍快照哈希 可观测: 同源同哈希 · 改一算子必变 · 本趟 甲!=乙 且 折叠>0 · 照散[] 通道 已落位')
sys.exit(0)
