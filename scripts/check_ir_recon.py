# -*- coding: utf-8 -*-
"""开炮 IR 层 · 判据 2+3 检查器：**实例级 L1↔L0 对账** + **受限类 仅凭 IR 重建（含顺序）**。

键 K = (类, 基=帧, **函号**, **槽**, **访问宽**)；类 ∈ {1 装载(1051), 2 存储(1052), 3 取址(1102)}；
槽 = L1 侧 `值表偏`(= 变量表.栈偏移) ⟷ L0 侧 `槽`(= 位移 + 帧大小)（同口径、与时刻无关）；函号 消 跨函数 槽碰撞。

**门（全部为 数字，非自指）**：
  ②-1 **键级 匹配率（L1 侧）≥ 95%**：L1 声明的每个 (类,函,槽,宽) 键，L0 侧 必须有 真发射（起点实测 98.0%）。
      未匹配的 L1 键 **逐键 打印 + 登记**（理由：IR 声明而编译路径未发射）。
  ②-2 **②-A 机械归因率 ≥ 95%**：L0 独有键（域内）必须能机械归因（同 (函,槽) 的 L1 键存在 ⟹ 槽复用/访问宽分歧）。
  ②-3 **实例级 未匹配 ≤ 5%**：Σ(L1-键 的 缺额) / L1 实例。
  ②-4 **C 侧自报 全字段 交叉核对**（声明/L0实际/键数/差异/盈余 —— 一项不符 即 FAIL）。
  ②-5 L0 记录 **不得溢出**（响亮拒绝）。
  ③-1 **键级 匹配率 ≥ 95%**（同 ②-1，重建 侧 口径）。
  ③-2 **顺序：跨键 偏序一致率 ≥ 95%**（若 L1 中 k1 全部先于 k2，则 L0 中亦然 —— 逐对 可证）。
      同 (函,类) 内 偏序 作 **诊断** 打印（登记：L1 按节序 / L0 按发射序，语句内 求值次序 可 重排）。
  ③-3 **三次 独立编译** 该段 **逐字节相同**。
  ★ 本器 **独立复算**（python 侧 从 dump 原始材料 重建 键/多重集/偏序），C 侧自报 只作 交叉核对。
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
RE_INST = re.compile(r'^\s+(?:抽|子)\[(\d+)\] 算子=(\d+) 现高=(\d+) 元=(-?\d+) 源一=(-?\d+) 源二=(-?\d+) (?:链头|后继)=(-?\d+)', re.M)
RE_VAL = re.compile(r'^\s+值\[(\d+)\] 实体=变量 名=(\S+) 变量序=(\d+) 存储类=(\d+) 宽=(\d+) 访=(\d+) 偏=(-?\d+) 型=(\d+) 函号=(\d+)$', re.M)
RE_L0 = re.compile(r'^\s+L0访\[(\d+)\] 类=(\d+) 基=(\d+) 偏=(-?\d+) 宽=(-?\d+) 帧=(-?\d+) 槽=(-?\d+) 函=(\d+)$', re.M)
类标 = {1051: 1, 1052: 2, 1102: 3}
门_键匹配 = 0.95        # ②-1 / ③-1
门_归因 = 0.95          # ②-2
门_实例 = 0.05          # ②-3
门_偏序 = 0.95          # ③-2


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
    l1 = []
    for m in RE_INST.finditer(txt):
        op = int(m.group(2))
        if op in 类标 and int(m.group(5)) >= base:
            v = vals.get(int(m.group(5)))
            if v and v['存'] in (1, 3):
                l1.append((类标[op], 1, v['函'], v['偏'], v['访']))
    l0 = [(int(m.group(2)), int(m.group(3)), int(m.group(8)), int(m.group(7)), int(m.group(5)))
          for m in RE_L0.finditer(txt)]
    return base, vals, l1, l0


def 偏序(seqA, seqB):
    首甲, 末甲, 首乙, 末乙 = {}, {}, {}, {}
    for i, k in seqA:
        首甲.setdefault(k, i)
        末甲[k] = i
    for i, k in seqB:
        首乙.setdefault(k, i)
        末乙[k] = i
    键 = [k for k in set(首甲) if k in 首乙]
    对 = 逆 = 0
    for x in range(len(键)):
        for y in range(x + 1, len(键)):
            k1, k2 = 键[x], 键[y]
            if 末甲[k1] < 首甲[k2]:
                对 += 1
                if not (末乙[k1] < 首乙[k2]):
                    逆 += 1
            elif 末甲[k2] < 首甲[k1]:
                对 += 1
                if not (末乙[k2] < 首乙[k1]):
                    逆 += 1
    return 对, 逆


def recompute(base, vals, l1, l0):
    甲, 乙 = Counter(l1), Counter(l0)
    甲例 = defaultdict(list)
    for k in l1:
        甲例[k].append(1)
    域 = set((k[2], k[3]) for k in 甲)
    kL0域 = set(k for k in 乙 if (k[2], k[3]) in 域)
    kL1 = set(甲)
    交集 = kL1 & set(乙)
    独有A = sorted(kL0域 - kL1)
    独有B = sorted(kL1 - set(乙))
    可归因A = [k for k in 独有A if any(x[2] == k[2] and x[3] == k[3] for x in kL1)]
    可归因B = [k for k in 独有B if any(x[2] == k[2] and x[3] == k[3] for x in 乙)]
    域外 = sum(v for k, v in 乙.items() if (k[2], k[3]) not in 域)
    短甲 = sorted(k for k in 甲 if 乙[k] < 甲[k])
    短数 = sum(甲[k] - 乙[k] for k in 短甲)
    对, 逆 = 偏序([(i, k) for i, k in enumerate(l1)], [(i, k) for i, k in enumerate(l0)])
    对2 = 逆2 = 0
    for 函 in set(k[2] for k in kL1):
        for c in (1, 2, 3):
            a = [(i, k) for i, k in enumerate(l1) if k[2] == 函 and k[0] == c]
            b = [(i, k) for i, k in enumerate(l0) if k[2] == 函 and k[0] == c]
            x, y = 偏序(a, b)
            对2 += x
            逆2 += y
    return dict(甲=甲, 乙=乙, 甲例=甲例, 域=域, kL1=kL1, kL0域=kL0域, 交集=交集,
                独有A=独有A, 独有B=独有B, 可归因A=可归因A, 可归因B=可归因B, 域外=域外,
                短甲=短甲, 短数=短数, M=len(l1), 对=对, 逆=逆, 对2=对2, 逆2=逆2)


def analyze(dumps, label, verbose=True):
    fails = []
    txt = '\n'.join(dumps)
    if '-- IR 实例级对账' not in txt:
        fails.append('%s: 缺 「实例级对账」段' % label)
    if '-- IR 受限类 重建' not in txt:
        fails.append('%s: 缺 「受限类重建」段' % label)
    base, vals, l1, l0 = parse(txt)
    r = recompute(base, vals, l1, l0)
    甲, 乙, kL1, kL0域, 交集 = r['甲'], r['乙'], r['kL1'], r['kL0域'], r['交集']
    if verbose:
        print('  值表 条目=%d · L1 声明 实例=%d（键 %d）· L0 实例=%d · 对账域 %d 个 (函,槽)'
              % (len(vals), len(l1), len(kL1), len(l0), len(r['域'])))
        键匹配 = 100.0 * len(交集) / max(1, len(kL1))
        print(' ② 实例级对账（键 =(类, 基=帧, 函号, 槽, 访问宽)；独立复算）:')
        print('     ②-1 **键级 匹配率（L1 侧）= %d/%d = %.1f%%**（门 ≥%.0f%%）· L1 独有键 = %d（逐键登记）'
              % (len(交集), len(kL1), 键匹配, 100 * 门_键匹配, len(r['独有B'])))
        for k in r['独有B'][:8]:
            print('        B 类=%d 函=%d 槽=%d 宽=%d : L1=%d' % (k[0], k[2], k[3], k[4], 甲[k]))
        print('     ②-2 域内 L0 独有键 = %d · **机械归因 = %d (%.1f%%)**（同 (函,槽) 的 L1 键存在 ⟹ 槽复用/访问宽分歧）'
              % (len(r['独有A']), len(r['可归因A']), 100.0 * len(r['可归因A']) / max(1, len(r['独有A']))))
        print('          L0 域内 实例 = %d · 域外 实例 = %d（编译器内部槽/序言/预置 —— 机械排除：槽 ∉ 域）'
              % (sum(乙[k] for k in kL0域), r['域外']))
        print('     ②-3 **实例级 未匹配 = %d 实例 = %.2f%% of L1 声明**（门 ≤%.0f%%）· 涉 %d 键'
              % (r['短数'], 100.0 * r['短数'] / max(1, len(l1)), 100 * 门_实例, len(r['短甲'])))
        for k in r['短甲'][:6]:
            print('        D 类=%d 函=%d 槽=%d 宽=%d : IR=%d L0=%d' % (k[0], k[2], k[3], k[4], 甲[k], 乙[k]))
        print(' ③ 受限类重建（仅 L1+值表）:')
        print('     ③-1 键级 匹配 = %.1f%%（同 ②-1）' % 键匹配)
        print('     ③-2 **顺序：跨键 偏序 一致率 = %d/%d = %.2f%%**（门 ≥%.0f%%）· 同 (函,类) 内 诊断 = %.2f%%'
              % (r['对'] - r['逆'], r['对'], 100.0 * (r['对'] - r['逆']) / max(1, r['对']), 100 * 门_偏序,
                 100.0 * (r['对2'] - r['逆2']) / max(1, r['对2'])))
    # ---- 门 ----
    if len(l1) <= 0:
        fails.append('② L1 声明 实例 = 0（量具 空转）')
    键匹配 = len(交集) / max(1, len(kL1))
    if 键匹配 < 门_键匹配:
        fails.append('②-1 键级 匹配率 %.1f%% < %.0f%%' % (100 * 键匹配, 100 * 门_键匹配))
    if r['独有A'] and len(r['可归因A']) < 门_归因 * len(r['独有A']):
        fails.append('②-2 归因率 %.1f%% < %.0f%%' % (100.0 * len(r['可归因A']) / len(r['独有A']), 100 * 门_归因))
    if r['短数'] > 门_实例 * len(l1):
        fails.append('②-3 实例级 未匹配 %d (%.2f%%) 超 门 %.0f%%' % (r['短数'], 100.0 * r['短数'] / len(l1), 100 * 门_实例))
    偏序率 = (r['对'] - r['逆']) / max(1, r['对'])
    if 偏序率 < 门_偏序:
        fails.append('③-2 跨键 偏序 一致率 %.2f%% < %.0f%%' % (100 * 偏序率, 100 * 门_偏序))
    if r['M'] <= 0:
        fails.append('③ 重建 序列 = 0（量具 空转）')
    # ---- ②-4 C 侧 自报 全字段 交叉核对 ----
    m1 = re.search(r'L1 声明 实例=(\d+) · L0 实际 实例=(\d+) · 键数=(\d+) · \*\*未登记差异\(L0<L1 的键\)=(\d+)\*\* · L0 盈余 实例=(\d+)', txt)
    if not m1:
        fails.append('②-4 未见 C 侧 对账 汇总行')
    else:
        声明, l0实, 键数, 差异, 盈 = (int(m1.group(i)) for i in range(1, 6))
        我声明, 我l0, 我键, 我差, 我盈 = (len(l1), sum(乙[k] for k in kL1), len(kL1), len(r['短甲']),
                                        sum(乙[k] - 甲[k] for k in 甲 if 乙[k] > 甲[k]))
        ok = (声明 == 我声明 and l0实 == 我l0 and 键数 == 我键 and 差异 == 我差 and 盈 == 我盈)
        if verbose:
            print(' ②-4 C 侧 自报=(%d,%d,%d,%d,%d) ⟷ 独立复算=(%d,%d,%d,%d,%d) %s'
                  % (声明, l0实, 键数, 差异, 盈, 我声明, 我l0, 我键, 我差, 我盈, '✓' if ok else '✗'))
        if not ok:
            fails.append('②-4 C 侧 自报 与 独立复算 不一致')
    m2 = re.search(r'重建 序列 条数=(\d+) · \*\*保序 命中=(\d+)\*\* · 未命中=(\d+)', txt)
    if not m2:
        fails.append('③ 未见 C 侧 重建 汇总行')
    elif int(m2.group(1)) != r['M']:
        fails.append('③ C 侧 重建 条数 %s != 独立复算 %d' % (m2.group(1), r['M']))
    if re.search(r'溢出=1', txt) or '[ERR] IR-L0REC-OVERFLOW' in txt:
        fails.append('②-5 L0 访问 实例记录 溢出（响亮拒绝）')
    return fails


def main():
    args = sys.argv[1:]
    cc0 = next((a for a in args if not a.startswith('--')), CC)
    if '--selftest' in args:
        raw = dump_once(cc0, 'sel')
        print('== 自测：5 组 破坏 ⟹ 期望 全部 FAIL ==')
        ok = True
        cases = []
        m = RE_VAL.search(raw)
        cases.append(('a 值表偏+4（IR 漂移）', raw[:m.start()] + m.group(0).replace(' 偏=%s' % m.group(7), ' 偏=%d' % (int(m.group(7)) + 4)) + raw[m.end():]))
        # 取 **域内** 的 L0 实例如作为 破坏对象（域外实例 被 机械排除，动了 本应 无影响）
        _b, _v, _l1, _l0 = parse(raw)
        _域 = set((k[2], k[3]) for k in _l1)
        _行 = None
        for mm in RE_L0.finditer(raw):
            if (int(mm.group(8)), int(mm.group(7))) in _域:
                _行 = mm
                break
        assert _行 is not None, '自测: 找不到 域内 L0 实例'
        cases.append(('b 删一条 **域内** L0 实例', raw[:_行.start()] + raw[_行.end() + 1:]))
        cases.append(('c 抹 C 侧 汇总行', re.sub(r'^.*L1 声明 实例=.*$', '', raw, flags=re.M)))
        inst = next(mm for mm in RE_INST.finditer(raw) if int(mm.group(2)) in 类标 and int(mm.group(5)) > 1048576)
        cases.append(('d L1 值引用 +1', raw[:inst.start()] + inst.group(0).replace(' 源一=%s' % inst.group(5), ' 源一=%d' % (int(inst.group(5)) + 1)) + raw[inst.end():]))
        cases.append(('e 改一条 **域内** L0 槽+8', raw[:_行.start()] + _行.group(0).replace(' 槽=%s' % _行.group(7), ' 槽=%d' % (int(_行.group(7)) + 8)) + raw[_行.end():]))
        for name, d in cases:
            f = analyze([d], name, verbose=False)
            print('   %-24s 判 FAIL ? %s  (%s)' % (name, len(f) > 0, f[0] if f else ''))
            ok = ok and len(f) > 0
        print('\n[%s] 自测：5 组 破坏 全部 FAIL ⟹ 判据 非空转' % ('PASS' if ok else 'FAIL'))
        sys.exit(0 if ok else 1)
    if '--dump-file' in args:
        f = args[args.index('--dump-file') + 1]
        txt = open(f, encoding='utf-8', errors='replace').read()
        fails = analyze([txt], os.path.basename(f))
    else:
        print('编译器 =', cc0)
        if not os.path.exists(cc0):
            print('[FAIL] 引擎不存在'); sys.exit(1)
        d1, d2, d3 = dump_once(cc0, 'a'), dump_once(cc0, 'b'), dump_once(cc0, 'c')
        def seg(t):
            i = t.find('-- IR 值表')
            j = t.find('-- IR pass 常量折叠')
            return t[i:j] if i >= 0 and j > i else ''
        same = (seg(d1) != '' and seg(d1) == seg(d2) == seg(d3))
        print(' ③-3 可重入（三次独立编译 「值表+对账+重建」段 逐字节相同）: %s' % ('✓' if same else '✗'))
        fails = analyze([d1], 'engine')
        if not same:
            fails.append('③-3 三次编译 该段 不逐字节相同（跨编译残留）')
    print()
    if fails:
        print('[FAIL] 判据 2/3 不成立:')
        for x in fails:
            print('   -', x)
        sys.exit(1)
    print('[PASS] 判据 2/3：键级匹配 ≥95% · A 归因 ≥95% · 实例未匹配 ≤5% · 跨键偏序 ≥95% · 自报全字段一致 · 三次逐字节同')
    sys.exit(0)


main()
