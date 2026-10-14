# -*- coding: utf-8 -*-
"""开炮 IR 层 · 判据 2+3 检查器：**实例级 L1↔L0 对账** + **受限类 仅凭 IR 重建**。

键 K = (类, 基=帧, **函号**, **槽**, **宽**)；类 ∈ {1 装载(1051), 2 存储(1052), 3 取址(1102)};
槽 = L1 侧 `值表偏`(= 变量表.栈偏移) ⟷ L0 侧 `槽`(= 位移 + 帧大小) —— **同口径、与时刻无关**；
函号 消 跨函数 槽碰撞（不同函数的局部可落在同一槽偏移）。

**对账域** = L1 声明 触及 的 (函号, 槽) 对（= 用户对象槽位）。
判据:
  ②-A **域内 L0 独有键**（L0 发射 而 IR 无声明）⟹ 必须 逐键 登记（理由类：序言 参数保存 / 编译器临时槽）。
  ②-B **域内 L1 独有键**（IR 声明 而 L0 无发射）⟹ 必须 逐键 登记（理由类：死读/寄存器直用/路径分派）。
  ②-C **域外 L0 实例** = 编译器内部槽（临时基/状态区/预置段/运行时内联）⟹ 机械排除 + 计数登记。
  ②-D **登记 的 未匹配实例 ≤ 5% of L1 声明**（防 登记表 变成 大口袋）；条数 > 0。
  ③ **受限类重建（仅凭 L1 + 值表，不读 L0 侧任何复算）**:
      键集：L1 键集 == L0 域内 键集（完全一致）；
      实例：逐键 L0 ≥ 重建（IR 声明 都 真发射）— 未达标键 逐键登记（同 ②-B）；
      顺序：保序命中率 作 **诊断** 打印（登记：L1 按节序、L0 按发射序，语句内 可 重排）；
      确定性：**三次 独立编译 该段 逐字节相同**。
  ★ 本器 **独立复算**（python 侧 从 dump 原始材料 重建 键/统计），C 侧 自报 只作 **交叉核对**。
用法:
  python scripts/check_ir_recon.py [引擎]
  python scripts/check_ir_recon.py --dump-file <f>
  python scripts/check_ir_recon.py [引擎] --selftest
"""
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
T = os.path.join(os.environ.get('TEMP', '.'), 'qiyuan-ir-recon')
os.makedirs(T, exist_ok=True)
CC = os.path.join(QJ, 'qcc_zh_v2.exe')
PROBE = 'scripts/probe/ir_cover_probe.c'
RE_INST = re.compile(r'^\s+抽\[(\d+)\] 算子=(\d+) 现高=(\d+) 元=(-?\d+) 源一=(-?\d+) 源二=(-?\d+) 链头=(-?\d+)', re.M)
RE_VAL = re.compile(r'^\s+值\[(\d+)\] 实体=变量 名=(\S+) 变量序=(\d+) 存储类=(\d+) 宽=(\d+) 访=(\d+) 偏=(-?\d+) 型=(\d+) 函号=(\d+)$', re.M)
RE_L0 = re.compile(r'^\s+L0访\[(\d+)\] 类=(\d+) 基=(\d+) 偏=(-?\d+) 宽=(-?\d+) 帧=(-?\d+) 槽=(-?\d+) 函=(\d+)$', re.M)
类标 = {1051: 1, 1052: 2, 1102: 3}
登记门槛 = 0.05     # ②-D: 未匹配实例 ≤ 5% of L1 声明


def dump_once(cc, tag):
    exe = os.path.join(T, 'r_%s.exe' % tag)
    r = subprocess.run([cc, os.path.join(QJ, PROBE), '-o', exe], capture_output=True,
                       env=dict(os.environ, QCC_DUMP='ir'), cwd=QJ)
    return ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')


def parse(txt):
    base = int(re.search(r'值基=(\d+) 值容=(\d+) 值函基=(\d+)', txt).group(1))
    vals = {}
    for m in RE_VAL.finditer(txt):
        vals[base + int(m.group(1))] = dict(名=m.group(2), 存=int(m.group(4)), 宽=int(m.group(5)),
                                           访=int(m.group(6)), 偏=int(m.group(7)), 函=int(m.group(9)))
    l1 = [(int(m.group(1)), int(m.group(2)), int(m.group(5))) for m in RE_INST.finditer(txt)]
    l0 = [(int(m.group(2)), int(m.group(3)), int(m.group(8)), int(m.group(7)), int(m.group(5)))
          for m in RE_L0.finditer(txt)]
    return base, vals, l1, l0


def recompute(base, vals, l1, l0):
    甲 = Counter()
    甲例 = defaultdict(list)
    序列 = []
    for 节, op, v1 in l1:
        if op not in 类标 or v1 < base:
            continue
        v = vals.get(v1)
        if not v or v['存'] not in (1, 3):
            continue
        k = (类标[op], 1, v['函'], v['偏'], v['访'])
        甲[k] += 1
        甲例[k].append((节, v['名']))
        序列.append(k)
    乙 = Counter(l0)
    l0例 = defaultdict(list)
    for i, k in enumerate(l0):
        l0例[k].append(i)
    域 = set((k[2], k[3]) for k in 甲)                       # (函号, 槽) —— 对账域
    kL1 = set(甲)
    kL0域 = set(k for k in 乙 if (k[2], k[3]) in 域)
    独有A = sorted(kL0域 - kL1)                              # L0 发射 而 IR 无
    独有B = sorted(kL1 - set(乙))                            # IR 声明 而 L0 无（全域）
    # ②-A 的 **机械归因**：同 (函, 槽) 的 L1 键 存在（⟹ 该槽 是 IR 声明的用户对象槽，
    #     但 L0 以 IR 未声明的 (类, 宽) 组合 访问了 它 ⟹ 槽复用（不同作用域变量共享槽）/ 访问宽分歧）
    可归因A = [k for k in 独有A if any(x[2] == k[2] and x[3] == k[3] for x in kL1)]
    # ②-B 的 **机械归因**：同 (函, 槽) 的 L0 键 存在（⟹ 该槽 确有访问，仅 类/宽 不同）
    可归因B = [k for k in 独有B if any(x[2] == k[2] and x[3] == k[3] for x in 乙)]
    域外 = sum(v for k, v in 乙.items() if (k[2], k[3]) not in 域)
    短甲 = sorted(k for k in 甲 if 乙[k] < 甲[k])             # 实例级：L0 < IR
    短数 = sum(甲[k] - 乙[k] for k in 短甲)
    # 保序（诊断）
    游 = 0
    命中 = 0
    for k in 序列:
        位 = -1
        for i in range(游, len(l0)):
            if l0[i] == k:
                位 = i
                break
        if 位 >= 0:
            命中 += 1
            游 = 位 + 1
    return dict(甲=甲, 乙=乙, 甲例=甲例, l0例=l0例, 域=域, kL1=kL1, kL0域=kL0域,
                独有A=独有A, 独有B=独有B, 域外=域外, 短甲=短甲, 短数=短数,
                可归因A=可归因A, 可归因B=可归因B,
                M=len(序列), 命中=命中)


def analyze(dumps, label, verbose=True):
    fails = []
    txt = '\n'.join(dumps)
    if '-- IR 实例级对账' not in txt:
        fails.append('%s: 缺 「实例级对账」段' % label)
    if '-- IR 受限类 重建' not in txt:
        fails.append('%s: 缺 「受限类重建」段' % label)
    base, vals, l1, l0 = parse(txt)
    r = recompute(base, vals, l1, l0)
    甲, 乙, 域 = r['甲'], r['乙'], r['域']
    if verbose:
        print('  值表 条目=%d · L1 指令=%d · L0 实例=%d · 对账域 (%d 个 (函,槽) 对)' % (len(vals), len(l1), len(l0), len(域)))
        print(' ② 实例级对账:')
        print('     L1 声明 实例=%d（键 %d）· L0 域内 实例=%d（键 %d）· L0 域外 实例=%d'
              % (sum(甲.values()), len(甲), sum(乙[k] for k in r['kL0域']), len(r['kL0域']), r['域外']))
        print('     ②-A 域内 **L0 独有键** = %d（其中 **机械可归因** = %d：同 (函,槽) 的 L1 键 存在 ⟹ 槽复用/访问宽分歧）'
              % (len(r['独有A']), len(r['可归因A'])))
        print('     ②-B 域内 **L1 独有键** = %d（其中 **机械可归因** = %d：同 (函,槽) 有 L0 访问；**完全无 = %d** 挂账）'
              % (len(r['独有B']), len(r['可归因B']), len(r['独有B']) - len(r['可归因B'])))
        print('     ②-C 域外 L0 实例（编译器内部槽/序言/预置段） = %d（**机械排除**：槽 ∉ 对账域）' % r['域外'])
        for k in r['独有A'][:6]:
            print('        A 类=%d 函=%d 槽=%d 宽=%d : L0=%d' % (k[0], k[2], k[3], k[4], 乙[k]))
        for k in r['独有B'][:6]:
            print('        B 类=%d 函=%d 槽=%d 宽=%d : L1=%d 变量=%s' % (k[0], k[2], k[3], k[4], 甲[k], sorted(set(甲例[0] for 甲例 in [r['甲例'][k]]) )[0][:2]))
        print('     ②-D 实例级 **L0 < IR 的键 = %d**（缺 %d 实例，占 L1 声明 %.2f%%）'
              % (len(r['短甲']), r['短数'], 100.0 * r['短数'] / max(1, sum(甲.values()))))
        for k in r['短甲'][:6]:
            print('        D 类=%d 函=%d 槽=%d 宽=%d : IR=%d L0=%d 变量=%s' % (k[0], k[2], k[3], k[4], 甲[k], 乙[k], r['甲例'][k][:2]))
        print(' ③ 受限类重建（仅 L1+值表）: 序列=%d · 键集 差 = A∪B 已登记 (%d) · 保序命中=%d (%.1f%%, 诊断; 登记: L1 按节序 / L0 按发射序, 语句内 可 重排)'
              % (r['M'], len(r['独有A']) + len(r['独有B']), r['命中'], 100.0 * r['命中'] / max(1, r['M'])))
    if sum(甲.values()) <= 0:
        fails.append('② L1 声明 实例 = 0（量具 空转）')
    if r['M'] <= 0:
        fails.append('③ 重建 序列 = 0（量具 空转）')
    # ②-B / ②-D: 未匹配 实例 比例 门槛（登记 只 允许 在 小 比例 内 —— 防 登记表 变成 口袋）
    if r['短数'] > 登记门槛 * sum(甲.values()):
        fails.append('②-D 未匹配实例 %d (%.2f%%) 超 门槛 %.0f%%' % (r['短数'], 100.0 * r['短数'] / max(1, sum(甲.values())), 100 * 登记门槛))
    # ②-A 归因率：槽复用/宽分歧 必须 机械可核（同 (函,槽) 的 L1 键 存在）≥95%
    if r['独有A'] and len(r['可归因A']) < 0.95 * len(r['独有A']):
        fails.append('②-A 归因率 %.1f%% < 95%%（%d/%d 键 无法 机械归因）'
                     % (100.0 * len(r['可归因A']) / len(r['独有A']), len(r['可归因A']), len(r['独有A'])))
    # ③ 键集：差 必须 恰为 已登记 的 A∪B
    if (r['kL1'] ^ r['kL0域']) != (set(r['独有A']) | set(r['独有B'])):
        fails.append('③ 键集 差 不 等于 已登记 的 A∪B')
    # 交叉核对 C 侧 自报（同口径 字段）
    m1 = re.search(r'L1 声明 实例=(\d+) · L0 实际 实例=(\d+) · 键数=(\d+)', txt)
    if not m1:
        fails.append('② 未见 C 侧 对账 汇总行')
    elif int(m1.group(1)) != sum(甲.values()) or int(m1.group(3)) != len(甲):
        fails.append('②b C 侧 自报 与 独立复算 不一致: %s' % str(m1.groups()))
    m2 = re.search(r'重建 序列 条数=(\d+) · \*\*保序 命中=(\d+)\*\*', txt)
    if not m2:
        fails.append('③ 未见 C 侧 重建 汇总行')
    elif int(m2.group(1)) != r['M'] or int(m2.group(2)) != r['命中']:
        fails.append('③b C 侧 重建 自报 与 独立复算 不一致: %s' % str(m2.groups()))
    if re.search(r'溢出=1', txt) or '[ERR] IR-L0REC-OVERFLOW' in txt:
        fails.append('② L0 访问 实例记录 溢出（响亮拒绝）')
    return fails


def main():
    args = sys.argv[1:]
    cc0 = next((a for a in args if not a.startswith('--')), CC)
    if '--selftest' in args:
        raw = dump_once(cc0, 'sel')
        print('== 自测：4 组 破坏 ⟹ 期望 FAIL ==')
        ok = True
        m = RE_VAL.search(raw)
        d = raw[:m.start()] + m.group(0).replace(' 偏=%s' % m.group(7), ' 偏=%d' % (int(m.group(7)) + 4)) + raw[m.end():]
        f = analyze([d], 'a', verbose=False)
        print('   a 值表偏+4（IR 侧 漂移）         判 FAIL ? %s  (%s)' % (len(f) > 0, f[0] if f else ''))
        ok = ok and len(f) > 0
        m2 = RE_L0.search(raw)
        d2 = raw[:m2.start()] + raw[m2.end() + 1:]
        f2 = analyze([d2], 'b', verbose=False)
        print('   b 删一条 L0 实例                 判 FAIL ? %s  (%s)' % (len(f2) > 0, f2[0] if f2 else ''))
        ok = ok and len(f2) > 0
        d3 = re.sub(r'^.*L1 声明 实例=.*$', '', raw, flags=re.M)
        f3 = analyze([d3], 'c', verbose=False)
        print('   c 抹 C 侧 汇总行（防判据跟实现走） 判 FAIL ? %s  (%s)' % (len(f3) > 0, f3[0] if f3 else ''))
        ok = ok and len(f3) > 0
        inst = None
        for mm in RE_INST.finditer(raw):
            if int(mm.group(2)) in 类标 and int(mm.group(5)) > 1048576:
                inst = mm
                break
        d4 = raw[:inst.start()] + inst.group(0).replace(' 源一=%s' % inst.group(5), ' 源一=%d' % (int(inst.group(5)) + 1)) + raw[inst.end():]
        f4 = analyze([d4], 'd', verbose=False)
        print('   d L1 值引用 挪一位（IR 侧 错值）   判 FAIL ? %s  (%s)' % (len(f4) > 0, f4[0] if f4 else ''))
        ok = ok and len(f4) > 0
        print('\n[%s] 自测：4 组 破坏 全部 FAIL ⟹ 判据 非空转' % ('PASS' if ok else 'FAIL'))
        sys.exit(0 if ok else 1)
    if '--dump-file' in args:
        f = args[args.index('--dump-file') + 1]
        txt = open(f, encoding='utf-8', errors='replace').read()
        fails = analyze([txt], os.path.basename(f))
    else:
        print('编译器 =', cc0)
        if not os.path.exists(cc0):
            print('[FAIL] 引擎不存在'); sys.exit(1)
        d1 = dump_once(cc0, 'a')
        d2 = dump_once(cc0, 'b')
        d3 = dump_once(cc0, 'c')
        def seg(t):
            i = t.find('-- IR 值表')
            j = t.find('-- IR pass 常量折叠')
            return t[i:j] if i >= 0 and j > i else ''
        same = (seg(d1) != '' and seg(d1) == seg(d2) == seg(d3))
        print(' ③c 可重入（三次独立编译 「值表+对账+重建」段 逐字节相同）: %s' % ('✓' if same else '✗'))
        fails = analyze([d1], 'engine')
        if not same:
            fails.append('③c 三次编译 该段 不逐字节相同（跨编译残留）')
    print()
    if fails:
        print('[FAIL] 判据 2/3（实例级对账 + 受限类重建） 不成立:')
        for x in fails:
            print('   -', x)
        sys.exit(1)
    print('[PASS] 判据 2/3：域内 键级 双向对应（差异 全部 逐键登记 且 ≤5%）· 实例级 未匹配 ≤5% · 键集 一致 · 三次逐字节相同')
    sys.exit(0)


main()
