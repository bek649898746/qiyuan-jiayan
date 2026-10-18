# -*- coding: utf-8 -*-
"""check_t5_landing.py —— T5「P0 接线」三项**真写入**判据器。

清单 T5 原文：「`溢出查标`/`空指针查标` → 值级属性位」·「`#123` 细化类型 (低,高) → `左戊一`/`半退三`」·
「`度散`（父类型/继承接口）→ `度散[目低]`」，并称「接线成本≈0，全场性价比最高」。

判据（**逐项读真值，不接受「定义存在」**；驱动真实终态引擎 dump，不重实现）:
  T5-1 值级属性位: dump 出现 `现高=` **>= 256** 的值条目（256=溢出位8 / 512=空指针位9）
  T5-2 #123 细化类型: dump 出现 `型[..] 细化(..) 下界=<非0> 上界=<非0>`（非默认才打印）
  T5-3 父类型 度散: dump 出现 `型[..] .. 父型=<非0>`
  **对照组**: 未开两项检查的常规 IR 探针 ⟹ 上述三项均**不得**出现（证明是真触发而非环境假象）
退出码: 0 = 三项各自读到真写入 且 对照组为零
"""
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(QJ)
TMP = os.path.join(os.environ.get('TEMP', '.'), '_t5_landing')
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
PROBE = os.path.join('scripts', 'probe', 't5_landing_probe.c')
CTRL = os.path.join('scripts', 'probe', 'ir_cover_probe.c')


def dump(src, tag):
    r = subprocess.run([CC, src, '-o', os.path.join(TMP, 't5_%s.exe' % tag)],
                       capture_output=True, cwd=QJ, env=dict(os.environ, QCC_DUMP='ir'))
    return ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace'), r.returncode


RE_GAO = re.compile(r'现高=(\d+)')
RE_XING = re.compile(r'型\[(\d+)\]\s+(\S+)\s+下界=(-?\d+)\s+上界=(-?\d+)\s+父型=(-?\d+)')

t, rc = dump(PROBE, 'main')
if rc != 0:
    print('[FAIL] 探针 编译 失败 rc=%d\n%s' % (rc, t[:400]))
    sys.exit(1)
lines = t.split('\n')

gao = [int(m.group(1)) for m in (RE_GAO.search(l) for l in lines) if m]
gao_big = [v for v in gao if v >= 256]
xing = [RE_XING.search(l).groups() for l in lines if RE_XING.search(l)]
xi_hua = [g for g in xing if '细化' in g[1]]
xi_fu = [g for g in xing if int(g[4]) != 0]

fails = []
print('[check_t5_landing] 编译器 = %s' % CC)

ok1 = len(gao_big) > 0
print('  T5-1 值级属性位: 现高>=256 的条目 = %d （例 %s）⟹ %s'
      % (len(gao_big), sorted(set(gao_big))[:4], '✓' if ok1 else '✗'))
if not ok1:
    fails.append('T5-1 未见 现高>=256（溢出/空指针属性位 未真写入）')

ok2 = len(xi_hua) > 0 and int(xi_hua[0][2]) != 0 and int(xi_hua[0][3]) != 0
print('  T5-2 #123 细化类型: 细化条目 = %d  %s ⟹ %s'
      % (len(xi_hua), xi_hua[0][1:] if xi_hua else '-', '✓' if ok2 else '✗'))
if not ok2:
    fails.append('T5-2 未见 细化类型 (低,高) 真写入 左戊一/半退三')

ok3 = len(xi_fu) > 0
print('  T5-3 父类型 度散: 父型!=0 的条目 = %d （例 型[%s] 父型=%s）⟹ %s'
      % (len(xi_fu), xi_fu[0][0] if xi_fu else '-', xi_fu[0][4] if xi_fu else '-', '✓' if ok3 else '✗'))
if not ok3:
    fails.append('T5-3 未见 度散 真写入（父型 全 0）')

# 对照组：常规 IR 探针（未开两项检查）⟹ 三项均不得出现
tc, rcc = dump(CTRL, 'ctrl')
cl = tc.split('\n')
c_gao = [int(RE_GAO.search(l).group(1)) for l in cl if RE_GAO.search(l)]
c_big = [v for v in c_gao if v >= 256]
c_xing = [l for l in cl if RE_XING.search(l)]
ok4 = (len(c_big) == 0 and len(c_xing) == 0)
print('  对照组（未开检查/无细化无继承）: 现高>=256 = %d · 型[] 行 = %d ⟹ %s'
      % (len(c_big), len(c_xing), '✓' if ok4 else '✗ 假阳性'))
if not ok4:
    fails.append('对照组 出现 三项真写入 ⟹ 判据可能 是 环境假象')

print()
if fails:
    print('[FAIL] T5 三项落位 判据 不成立:')
    for f in fails:
        print('   -', f)
    sys.exit(1)
print('[PASS] T5 三项真写入: 值级属性位（现高>=256）· #123 细化 (低,高) · 父类型 度散；且对照组为零')
sys.exit(0)
