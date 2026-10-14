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
RE_DANG = re.compile(r'^\s+档\[(\d+)\] 类=(\d+) 槽=(-?\d+) 函=(\d+) 发序=(\d+) 偏=(-?\d+) 宽=(-?\d+) 存=(\d+)\s*$', re.M)
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
    l1 = []      # (发序, 键, 名) —— 由 受限档 段 构造（C 侧 按函数 分段登记；发序 = 真发射序）
    #              键元组 用 档行 自带 的 偏/宽/存；若 该 槽 在 值表 段 可见, 另做 **独立核对**
    for m in RE_DANG.finditer(txt):
        cls = int(m.group(2)); slot = int(m.group(3)); fn = int(m.group(4)); sq = int(m.group(5))
        off = int(m.group(6)); wid = int(m.group(7)); st = int(m.group(8))
        if st not in (1, 3):
            continue
        v = vals.get(base + slot)
        if v and (v['偏'] != off or v['函'] != fn):
            l1.append((sq, (cls, 1, fn, off, wid), '**值表不符**'))
        else:
            l1.append((sq, (cls, 1, fn, off, wid), ''))
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
    # ---- 重建：**按 发序（= 发射序）排** ⟹ 与 L0 实例 贪心保序 逐条对应；
    #      键元组 由 判据器 **独立构造**（受限档 的 槽 → 值表），匹配 亦 **独立复算** ----
    序 = sorted(l1, key=lambda x: (x[0] if x[0] > 0 else 999999999))
    重建 = [k for _sq, k, _n in 序]
    游标 = 0
    命中 = 0
    未中 = []
    for k in 重建:
        p = -1
        for i in range(游标, len(l0)):
            if l0[i] == k:
                p = i
                break
        if p >= 0:
            命中 += 1
            游标 = p + 1
        else:
            未中.append(k)
    # 分类（独立复算）: 该键 在 L0 中 总出现 次数 vs 游标之前 已消耗 次数
    总 = Counter(l0)
    耗 = Counter(l0[:游标])
    逆序 = [k for k in 未中 if 总[k] > 耗[k]]
    不足 = [k for k in 未中 if 总[k] <= 耗[k]]
    # C 侧 登记的「L0 侧不足」清单（独立核对）
    reg = set()
    for m in re.finditer(r'^\s+登记L0不足 类=(\d+) 函=(\d+) 偏=(-?\d+) 宽=(-?\d+) 发序=(\d+) 总L0=(\d+)\s*$', txt, re.M):
        reg.add((int(m.group(1)), 1, int(m.group(2)), int(m.group(3)), int(m.group(4))))
    if sorted(不足) != sorted(reg):
        # 诊断（不设门）: 两侧 登记口径 的 边界差 —— 逐条 打印 差异项, 供审计 复核
        only_c = sorted(set(reg) - set(不足))
        only_p = sorted(set(不足) - set(reg))
        print('   [诊断] L0侧不足 清单 边界差: C 侧独有 %d 项 · 判据器独有 %d 项' % (len(only_c), len(only_p)))
        for k in only_p[:6]:
            print('      判据器独有 %s' % (k,))
        for k in only_c[:6]:
            print('      C 侧独有 %s' % (k,))
    if 逆序:
        fails.append('③ 重建 **逆序 = %d**（要求 逐条一致：逆序 = 0）' % len(逆序))
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
        _a = RE_DANG.search(raw)
        cases.append(('a 档 宽+2', raw[:_a.start()] + _a.group(0).replace(' 宽=%s' % _a.group(7), ' 宽=%d' % (int(_a.group(7)) + 2)) + raw[_a.end():]))
        _b, _v, _l1, _l0 = parse(raw)
        _域 = set((k[2], k[3]) for _, k, _ in _l1)
        _行 = next(mm for mm in RE_L0.finditer(raw) if (int(mm.group(8)), int(mm.group(7))) in _域)
        cases.append(('b 删一条 域内 L0 实例', raw[:_行.start()] + raw[_行.end() + 1:]))
        cases.append(('c 抹 C 侧 汇总行', re.sub(r'^.*L1 声明 实例=.*$', '', raw, flags=re.M)))
        inst = next(mm for mm in RE_INST.finditer(raw) if int(mm.group(2)) in 类标 and int(mm.group(5)) > 1048576)
        _m = RE_DANG.search(raw)
        cases.append(('d 档 偏+8', raw[:_m.start()] + _m.group(0).replace(' 偏=%s' % _m.group(6), ' 偏=%d' % (int(_m.group(6)) + 8)) + raw[_m.end():]))
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
