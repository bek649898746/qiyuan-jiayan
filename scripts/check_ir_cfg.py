# -*- coding: utf-8 -*-
"""R715N 刀三 v0 判据检查器（读 `QCC_DUMP=ir` 的 dump）：
   ① 每块 始 <= 终一
   ② 自洽尺: 块表 **无缝铺满** 节点序 (块[0].始==0 · 块[k+1].始 == 块[k].终一+1) + 后继 ∈ [0,上退)
   ③ 回边=1 的块数 == `控式==1` 的节点数
   ④ 零产物影响 → 由 尺1 直译全等 单独证（本器不判）
   附: 报告 非落空边数 (goto 真边) 与 首控式 分布。"""
import os, re, subprocess, sys
from collections import Counter

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
T = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'probe')  # R821e: 探针 随仓走（原 只读 %TEMP%/qiyuan-ir-knives ⇹ 目录被清理后 假红）
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(QJ, 'qcc_zh_v2.exe')

fails = []
env = dict(os.environ, QCC_DUMP='ir')
for name in sys.argv[2:] or ['kw3_b', 'kw3_a']:
    src = os.path.join(T, name + '.c')
    r = subprocess.run([CC, src, '-o', os.path.join(T, name + '.exe')], capture_output=True, env=env, cwd=QJ)
    out = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    print('==== %s  rc=%d ====' % (name, r.returncode))
    m = re.search(r'IR CFG 块表 .*?上退=(\d+)', out)
    if not m:
        print('   ✗ 无 CFG 段'); fails.append('%s 无 CFG 段' % name); continue
    nblk = int(m.group(1))
    blk = re.findall(r'块\[(\d+)\] 始=(-?\d+) 终一=(-?\d+) 后继数=(\d+) 后继0=(-?\d+) 回边=(\d+) 首控式=(\d+)', out)
    kong = re.findall(r'节\[(\d+)\] 控式=(\d+) 段双=(\d+)', out)
    blk = [(int(a), int(b), int(c), int(d), int(e), int(f), int(g)) for a, b, c, d, e, f, g in blk]
    print('   上退=%d | dump 块行=%d | 控式条目=%d' % (nblk, len(blk), len(kong)))
    # ①
    bad1 = [b for b in blk if b[1] > b[2]]
    # ②
    bad2 = []
    if blk and blk[0][1] != 0: bad2.append('块[0].始=%d (应 0)' % blk[0][1])
    for i in range(len(blk) - 1):
        if blk[i + 1][1] != blk[i][2] + 1:
            bad2.append('块[%d].始=%d != 块[%d].终一+1=%d' % (i + 1, blk[i + 1][1], i, blk[i][2] + 1))
    bad2 += ['块[%d] 后继0=%d 越界 (上退=%d)' % (b[0], b[4], nblk) for b in blk if b[3] > 0 and not (0 <= b[4] < nblk)]
    # ③
    n_loop = sum(1 for _, c, _ in kong if int(c) == 1)
    n_back = sum(1 for b in blk if b[5] == 1)
    ok3 = (n_loop == n_back)
    # 附
    nonfall = sum(1 for i, b in enumerate(blk) if b[3] > 0 and b[4] != i + 1)
    print('   ① 始<=终一: %s' % ('✓' if not bad1 else '✗ ' + str(bad1[:3])))
    print('   ② 无缝铺满+后继合法: %s' % ('✓' if not bad2 else '✗ ' + str(bad2[:3])))
    print('   ③ 回边数(%d) == 控式==1 节点数(%d): %s' % (n_back, n_loop, '✓' if ok3 else '✗'))
    print('   首控式 分布: %s | 非落空边(goto 真边) = %d' % (dict(sorted(Counter(b[6] for b in blk).items())), nonfall))
    if bad1: fails.append('%s 判据①' % name)
    if bad2: fails.append('%s 判据②' % name)
    if not ok3: fails.append('%s 判据③' % name)
print()
print('[%s] R715N 刀三 v0 判据①②③ %s' % ('PASS' if not fails else 'FAIL', '' if not fails else fails))
sys.exit(0 if not fails else 1)
