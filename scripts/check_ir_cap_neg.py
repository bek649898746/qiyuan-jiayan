#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_ir_cap_neg —— **L1 容量守卫 的端到端负锚 检查器**（刀六 AC3「响亮拒绝」的真证据）。

为什么需要它（R715G · 审计指出的 AC3 缺口）:
  L1 的两处容量守卫（`发抽象` 主槽 / `发子抽象` 子链）真容量 = 缓集/子链容 = **1,048,576**
  ⟹ 端到端触发需 **百万级 节点/子链条目** ⟹ 常规源不可行 ⟹ 守卫此前只有**静态判据**。
  本器 用 **调试开关** `QCC_DBG_IRCAP_MAIN` / `QCC_DBG_IRCAP_CHAIN`（把**阈值**缩到 8 / 4；
  **只缩阈值、不动数组上界** ⟹ 无越界写风险）驱动 **同一份 已发布 编译器** ⟹ 让 小源 也能撞上守卫。

判据（每个负锚 两条, 缺一不可）:
  ① **控制组**（不设开关）: rc == 0（正锚语义成立 ⟹ 证明「失败 由 开关 引入, 非源本身错」）
  ② **实验组**（设开关）    : rc != 0 **且** stderr 含 `[ERR] IR-OVERFLOW` ⟹ 该负锚 **通过**

用法: python scripts/check_ir_cap_neg.py [编译器]      (默认 自动定位: QCC_TEST_BIN > qcc_zh_v7..v1 > seed)
      R715J (2026-10-12 虾米): 原默认 `qcc_zh_r715.exe` 是**一次性发布名**，过期即 MISSING ⟹ 挂闸必假红。
      改**自定位**（照 `check_dwarf289.py::find_qcc` 成例，与闸④ 同工艺）⟹ 无参调用可用 ⟹ 可进闸。
退出码: 0 = 两负锚 4 条判据 全过; 1 = 有判据不成立
"""
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
TMP = os.path.join(os.environ.get('TEMP', '.'), '_ir_cap_neg')
os.makedirs(TMP, exist_ok=True)


def find_qcc():
    """自定位编译器（照 check_dwarf289.py 成例）。闸内 无参调用 必须 能自己找到。"""
    env = os.environ.get('QCC_TEST_BIN')
    if env and os.path.exists(env):
        return env
    for n in range(7, 0, -1):
        p = os.path.join(ROOT, 'qcc_zh_v%d.exe' % n)
        if os.path.exists(p):
            return p
    for nm in ('qcc_zh_seed.exe', 'qcc_x86.exe'):
        p = os.path.join(ROOT, nm)
        if os.path.exists(p):
            return p
    return os.path.join(ROOT, 'qcc_x86.exe')


CC = sys.argv[1] if len(sys.argv) > 1 else find_qcc()
CASES = [
    ('scripts/feat_zh_irland_l1cap_main_neg.c', 'QCC_DBG_IRCAP_MAIN', '主槽'),
    ('scripts/feat_zh_irland_l1cap_chain_neg.c', 'QCC_DBG_IRCAP_CHAIN', '子链'),
]

fails = []
print('[check_ir_cap_neg] 编译器 = %s' % CC)
for src, knob, tag in CASES:
    exe = os.path.join(TMP, 'capneg_%s.exe' % tag)
    # ① 控制组
    env0 = dict(os.environ)
    env0.pop(knob, None)
    r0 = subprocess.run([CC, src, '-o', exe], capture_output=True, cwd=ROOT, env=env0)
    t0 = ((r0.stdout or b'') + (r0.stderr or b'')).decode('utf-8', 'replace')
    run0 = ''
    if r0.returncode == 0 and os.path.exists(exe):
        run0 = subprocess.run([exe], capture_output=True).stdout.decode('utf-8', 'replace').strip()
    ok0 = (r0.returncode == 0)
    if not ok0:
        fails.append('%s 控制组 rc=%d (应 0): %s' % (tag, r0.returncode, t0.strip()[:120]))
    # ② 实验组
    env1 = dict(os.environ, **{knob: '1'})
    r1 = subprocess.run([CC, src, '-o', os.path.join(TMP, 'capneg_%s_k.exe' % tag)],
                        capture_output=True, cwd=ROOT, env=env1)
    t1 = ((r1.stdout or b'') + (r1.stderr or b'')).decode('utf-8', 'replace')
    hit = '[ERR] IR-OVERFLOW' in t1
    ok1 = (r1.returncode != 0) and hit
    if not ok1:
        fails.append('%s 实验组 rc=%d 含IR-OVERFLOW=%s (应 rc≠0 且 含)' % (tag, r1.returncode, hit))
    line = ([l for l in t1.split('\n') if '[ERR]' in l] or ['(无 [ERR] 行)'])[0]
    print('  %-4s 控制组 rc=%d %-22s │ 实验组(%s) rc=%d %s' % (
        tag, r0.returncode, repr(run0[:20]), knob, r1.returncode, line.strip()[:80]))

print()
if fails:
    print('[FAIL] %d 条判据不成立:' % len(fails))
    for f in fails:
        print('   -', f)
    sys.exit(1)
print('[PASS] 两负锚 × 2 判据 全过 ⟹ **容量守卫 端到端 响亮拒绝 成立**（错误码 IR-OVERFLOW + 非零退出）')
sys.exit(0)
