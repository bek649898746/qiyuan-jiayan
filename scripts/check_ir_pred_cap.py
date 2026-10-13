# -*- coding: utf-8 -*-
"""R720b 判据检查器（读 `QCC_DUMP=ir` 的 dump）· **本刀 自定 · 只取 严 判据**:
   ④ 前驱表 容量: 「容量自查」行 必须 `溢出块 = 0`（= 每个块的 **全部** 前驱 都 落槽 ⟹ 前驱表 完整）
   ⑤ 双向一致: 「前驱/后继 双向一致」行 必须 `违例 = 0`（且 应查 > 0 ⟹ 防 假绿/落空）
   ⚠️ 判据 ④ 是 ⑤ 的 **前置可信条件**: 溢出块 > 0 时 前驱表 有 截断 ⟹ ⑤ 的「成立」不可信 ⟹ 本器 直接 判 FAIL。
   用法: python check_ir_pred_cap.py [qcc引擎.exe] [源.c ...]
"""
import os, re, subprocess, sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
T = os.path.join(os.environ['TEMP'], 'qiyuan-ir-knives')          # 探针 输出目录 (产物)
P = os.path.join(QJ, 'scripts', 'probe')                          # 探针 **源** (入仓, 可复现)
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(QJ, 'qcc_zh_v2.exe')
if not os.path.isabs(CC):
    CC = os.path.join(QJ, CC)          # Windows: 相对名 按 父进程 cwd 解析 ⟹ 显式 拼 工作区
os.makedirs(T, exist_ok=True)

fails = []
env = dict(os.environ, QCC_DUMP='ir')
for name in sys.argv[2:] or ['kw3_b', 'kw3_a']:
    src = os.path.join(P, name + '.c')
    if not os.path.exists(src):
        fails.append('%s 探针源 缺失 (%s)' % (name, src)); continue
    r = subprocess.run([CC, src, '-o', os.path.join(T, name + '.exe')], capture_output=True, env=env, cwd=QJ)
    out = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    print('==== %s  rc=%d ====' % (name, r.returncode))

    m = re.search(r'前驱表 容量自查 .*?槽容=(\d+) · 块数=(\d+) · 实测 最大入度=(\d+) · 溢出块=(\d+)', out)
    if not m:
        print('   ✗ 无 容量自查 段'); fails.append('%s 无 容量自查' % name); continue
    slot, nblk, maxin, ovf = (int(x) for x in m.groups())
    ok4 = (ovf == 0) and (maxin <= slot)
    print('   ④ 容量: 槽容=%d 块数=%d 实测最大入度=%d 溢出块=%d ⟹ %s'
          % (slot, nblk, maxin, ovf, '✓ 完整' if ok4 else '✗ 有截断'))

    m2 = re.search(r'前驱/后继 双向一致 .*?应查=(\d+) 违例=(\d+) ⟹ (\S+)', out)
    if not m2:
        print('   ✗ 无 双向一致 段'); fails.append('%s 无 双向一致' % name); continue
    should, viol, verdict = int(m2.group(1)), int(m2.group(2)), m2.group(3)
    ok5 = (viol == 0) and (should > 0) and ok4   # 应查>0 防落空; ok4 是可信前置
    print('   ⑤ 双向一致: 应查=%d 违例=%d dump判=%s ⟹ %s'
          % (should, viol, verdict, '✓ 成立' if ok5 else '✗ 不成立(或 容量不可信)'))

    if not ok4: fails.append('%s 判据④ 溢出块=%d' % (name, ovf))
    if not ok5: fails.append('%s 判据⑤ 应查=%d 违例=%d' % (name, should, viol))

print()
print('[%s] R720b 前驱表 判据④(容量满)⑤(双向一致) %s' % ('PASS' if not fails else 'FAIL', '' if not fails else fails))
sys.exit(0 if not fails else 1)
