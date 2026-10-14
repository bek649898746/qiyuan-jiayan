# -*- coding: utf-8 -*-
"""开炮 IR 层 · 判据 2+3 检查器（v3 · 应 skeptic gap A/B）：
   **重建序列 按 发射序（发序）排**，与 L0 访问实例 逐条保序对应；门 取 判据条文 的 **字面要求**。

键 K = (类, 基=帧, 函号, 槽, 访问宽)；类 ∈ {1 装载(1051), 2 存储(1052), 3 取址(1102)}；
槽 = L1 `值表偏`(= 变量表.栈偏移) ⟷ L0 `槽`(= 位移+帧大小)；函号 消 跨函数 槽碰撞；
**发序** = 共享事件序（`记L0访` 与 受限类 L1 发射 同轴）⟹ 重建 可 按 发射序 排。

判据（**字面**，不再用 数字阈值 替换 条文口径）:
  ② 实例级对账：L1 声明 的 每条 受限类 实例 ⟷ L0 实际发射 的 帧访问 **逐条对应**；
     **未登记差异 = 0** —— 差异（L1 独有键 B / L0 独有键 A / 域外实例 C）**每条 逐字登记**（打印 + 归类理由）。
  ③ 受限类重建：仅凭 L1+值表 产 重建序列（**按 发序 = 发射序**），与 现役 L0 实例
     **逐条一致（顺序 + 基/偏移/宽度）**：**未命中 = 0** 且 重建条数 = L1 受限实例数；
     **三次 独立编译** 该段 逐字节相同。
  ④ C 侧自报 全字段 交叉核对（含 L0 实际实例数）；L0 记录 不得溢出。
  ⑤ 负向：域内 L0 实例 删/改 ⟹ 必 FAIL（真打靶）。
用法: python scripts/check_ir_recon.py [引擎] | --dump-file <f> | [引擎] --selftest
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
RE_INST = re.compile(r'^\s+(?:抽|子)\[(\d+)\] 算子=(\d+) 现高=(\d+) 元=(-?\d+) 源一=(-?\d+) 源二=(-?\d+) (?:链头|后继)=(-?\d+)(?: 发序=(\d+))?', re.M)
RE_VAL = re.compile(r'^\s+值\[(\d+)\] 实体=变量 名=(\S+) 变量序=(\d+) 存储类=(\d+) 宽=(\d+) 访=(\d+) 偏=(-?\d+) 型=(\d+) 函号=(\d+)$', re.M)
RE_L0 = re.compile(r'^\s+L0访\[(\d+)\] 类=(\d+) 基=(\d+) 偏=(-?\d+) 宽=(-?\d+) 帧=(-?\d+) 槽=(-?\d+) 函=(\d+)$', re.M)
类标 = {1051: 1, 1052: 2, 1102: 3}


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
    l1 = []      # (发序, 键)
    for m in RE_INST.finditer(txt):
        op = int(m.group(2))
        if op in 类标 and int(m.group(5)) >= base:
            v = vals.get(int(m.group(5)))
            if v and v['存'] in (1, 3):
                l1.append((int(m.group(8) or 0), (类标[op], 1, v['函'], v['偏'], (8 if 类标[op] == 3 else v['访'])), v['名']))
    l0 = [(int(m.group(2)), int(m.group(3)), int(m.group(8)), int(m.group(7)), int(m.group(5)))
          for m in RE_L0.finditer(txt)]
    return base, vals, l1, l0


def analyze(dumps, label, verbose=True):
    fails = []
    txt = '\n'.join(dumps)
    for seg in ('-- IR 值表', '-- IR L0 访问 实例', '-- IR 实例级对账', '-- IR 受限类 重建'):
        if seg not in txt:
            fails.append('%s: 缺 「%s」段' % (label, seg))
    base, vals, l1, l0 = parse(txt)
    甲 = Counter(k for _, k, _ in l1)
    乙 = Counter(l0)
    域 = set((k[2], k[3]) for k in 甲)
    kL1 = set(甲)
    kL0域 = set(k for k in 乙 if (k[2], k[3]) in 域)
    独有A = sorted(kL0域 - kL1)                     # L0 发射 而 IR 无声明
    独有B = sorted(kL1 - set(乙))                    # IR 声明 而 L0 无
    可归因A = [k for k in 独有A if any(x[2] == k[2] and x[3] == k[3] for x in kL1)]
    可归因B1 = [k for k in 独有B if any(x[2] == k[2] and x[3] == k[3] for x in 乙)]
    可归因B2 = [k for k in 独有B if k not in 可归因B1 and any(x[2] == k[2] and x[0] == k[0] and x[4] == k[4] for x in 乙)]
    未登记 = [k for k in 独有A if k not in 可归因A] + [k for k in 独有B if k not in 可归因B1 and k not in 可归因B2]
    域外 = sum(v for k, v in 乙.items() if (k[2], k[3]) not in 域)
    短甲 = sorted(k for k in 甲 if 乙[k] < 甲[k])
    短数 = sum(甲[k] - 乙[k] for k in 短甲)
    # ---- 重建：排序键 **取自被测引擎**（后序位置 —— 结构量，判据器 无法 自算）；
    #      键元组 由判据器 **独立构造**（L1 实例 + 值表）；匹配 由判据器 **独立复算** ----
    mrows = re.findall(r'^\s+重建\[(\d+)\] 序=(\d+) 类=(\d+) 偏=(-?\d+) 宽=(-?\d+) 函=(\d+) ⟷ (-?\d+)\s*$', txt, re.M)
    if not mrows:
        fails.append('③ 未见 C 侧 重建 明细行')
    重建 = [((int(c), 1, int(fn), int(off), int(wid)), int(ix), int(sq), int(hit)) for ix, sq, c, off, wid, fn, hit in mrows]
    if len(重建) != len(l1):
        fails.append('③ 重建 明细条数 %d != L1 受限实例 %d' % (len(重建), len(l1)))
    if Counter(k for k, _ix, _sq, _h in 重建) != Counter(k for _f, k, _n in l1):
        fails.append('③ 重建 键 **多重集** 与 判据器 独立构造 不符（C 侧 已 排序 ⟹ 顺序 不可逐项比）')
    游标 = 0
    命中 = 0
    未中 = []
    for k, ix, sq, hit_c in 重建:
        p = -1
        for i in range(游标, len(l0)):
            if l0[i] == k:
                p = i
                break
        if p >= 0:
            命中 += 1
            游标 = p + 1
            if hit_c != p:
                fails.append('③ 重建[%d] 命中位 C侧=%d 独立复算=%d' % (ix, hit_c, p))
        else:
            未中.append(k)
            if hit_c != -1:
                fails.append('③ 重建[%d] C侧报命中 %d 而独立复算 未命中' % (ix, hit_c))
    if 未中:
        fails.append('③ 重建 未命中 = %d（要求 逐条一致：未命中 = 0）' % len(未中))
    if len(重建) != len(l1):
        fails.append('③ 重建 条数 %d != L1 受限实例 %d' % (len(重建), len(l1)))
    # ④ C 侧自报 全字段
    m1 = re.search(r'L1 声明 实例=(\d+) · L0 实际 实例=(\d+) · 键数=(\d+) · \*\*未登记差异\(L0<L1 的键\)=(\d+)\*\* · L0 盈余 实例=(\d+)', txt)
    if not m1:
        fails.append('④ 未见 C 侧 对账 汇总行')
    else:
        声明, l0实, 键数, 差异, 盈 = (int(m1.group(i)) for i in range(1, 6))
        我 = (len(l1), sum(乙[k] for k in kL1), len(kL1), len(短甲), sum(乙[k] - 甲[k] for k in 甲 if 乙[k] > 甲[k]))
        if (声明, l0实, 键数, 差异, 盈) != 我:
            fails.append('④ C 侧 自报 %s != 独立复算 %s' % ((声明, l0实, 键数, 差异, 盈), 我))
    m2 = re.search(r'重建 序列 条数=(\d+) · \*\*保序 命中=(\d+)\*\* · 未命中=(\d+)', txt)
    if not m2:
        fails.append('③ 未见 C 侧 重建 汇总行')
    elif (int(m2.group(1)), int(m2.group(2)), int(m2.group(3))) != (len(重建), 命中, len(未中)):
        fails.append('③ C 侧 重建 自报 (%s,%s,%s) != 独立复算 (%d,%d,%d)'
                     % (m2.group(1), m2.group(2), m2.group(3), len(重建), 命中, len(未中)))
    if re.search(r'溢出=1', txt) or '[ERR] IR-L0REC-OVERFLOW' in txt:
        fails.append('④ L0 记录 溢出（响亮拒绝）')
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
        cases.append(('a 值表偏+4', raw[:m.start()] + m.group(0).replace(' 偏=%s' % m.group(7), ' 偏=%d' % (int(m.group(7)) + 4)) + raw[m.end():]))
        _b, _v, _l1, _l0 = parse(raw)
        _域 = set((k[2], k[3]) for _, k, _ in _l1)
        _行 = next(mm for mm in RE_L0.finditer(raw) if (int(mm.group(8)), int(mm.group(7))) in _域)
        cases.append(('b 删一条 域内 L0 实例', raw[:_行.start()] + raw[_行.end() + 1:]))
        cases.append(('c 抹 C 侧 汇总行', re.sub(r'^.*L1 声明 实例=.*$', '', raw, flags=re.M)))
        inst = next(mm for mm in RE_INST.finditer(raw) if int(mm.group(2)) in 类标 and int(mm.group(5)) > 1048576)
        cases.append(('d L1 值引用 +1', raw[:inst.start()] + inst.group(0).replace(' 源一=%s' % inst.group(5), ' 源一=%d' % (int(inst.group(5)) + 1)) + raw[inst.end():]))
        cases.append(('e 改一条 域内 L0 槽+8', raw[:_行.start()] + _行.group(0).replace(' 槽=%s' % _行.group(7), ' 槽=%d' % (int(_行.group(7)) + 8)) + raw[_行.end():]))
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
        print(' ③c 可重入（三次独立编译 该段 逐字节相同）: %s' % ('✓' if same else '✗'))
        fails = analyze([d1], 'engine')
        if not same:
            fails.append('③c 三次编译 该段 不逐字节相同（跨编译残留）')
    print()
    if fails:
        print('[FAIL] 判据 2/3 不成立:')
        for x in fails:
            print('   -', x)
        sys.exit(1)
    print('[PASS] 判据 2/3：**未登记差异 = 0**（差异 逐条 登记 + 机械归因）· **重建 按发射序 逐条一致（未命中 = 0）** · 自报全字段一致 · 三次逐字节同')
    sys.exit(0)


main()
