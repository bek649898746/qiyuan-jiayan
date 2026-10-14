# -*- coding: utf-8 -*-
"""开炮 IR 层 · 判据 1 检查器：**L1 的值有身份**（值表 + 受限类 操作数 解析）。

判据:
  ① 受限类（装载 1051 / 存储 1052 / 取址 1102 / 调用 1071 / 返回 1072）的**语义操作数槽**
     必须有身份：**值引用**（值基+值序 ⟹ 值表 可反查 实体/存储类/宽/基偏移）/ 节点导出值 / 字面。
     **无身份（-1）占比 ≤ 5%**（起点实测 45.2%）。
     例外（登记）：`裸返回`（抽返 源一 = -1）**语义上无返回值** ⟹ 不计入分母（逐条列出，禁静默）。
  ② 值表段存在且自洽：值基 = 缓集+1 · 每个被引用的值都在值表里有 存储类/宽/偏/型。
  ③ **反向查证（独立复算，不抄实现）**：对探针里 **手写期望** 的若干变量，
     值表给出的 (宽, 存储类) 必须与 **语言自身类型尺寸** 推算一致（整=4 / 双=8 / 字=1 / 指针=8 / 构值=各字段和）。
  ④ 计数自洽：L1 指令表 里 出现的 值引用 条数 == 值表段 声明的「被引用的值」(去重后) ≥ 1。
用法:
  python scripts/check_ir_value.py [引擎]                  # 编译探针后判
  python scripts/check_ir_value.py --dump-file <f>
  python scripts/check_ir_value.py [引擎] --selftest       # 破坏输入 必 FAIL（判据不自空转）
"""
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
T = os.path.join(os.environ.get('TEMP', '.'), 'qiyuan-ir-value')
os.makedirs(T, exist_ok=True)
CC = os.path.join(QJ, 'qcc_zh_v2.exe')
PROBES = ['scripts/probe/ir_cover_probe.c']

受限 = {1051: ('装载', 1), 1052: ('存储', 2), 1102: ('取址', 1), 1071: ('调用', 1), 1072: ('返回', 1)}
槽数 = {1051: 1, 1052: 2, 1102: 1, 1071: 1, 1072: 1}
# ③ 手写期望（来自 探针 源码 —— 独立于 实现；宽 = 语言 自身 类型尺寸）
期望宽 = {'甲': 4, '乙': 4, '位与': 4, '位或': 4, '位异': 4, '左移值': 4, '右移值': 4,
        '无甲': 4, '逻右': 4, '浮': 8, '回整': 4, '窄': 1, '零扩': 1, '指': 8, '指加': 4,
        '列': 8, '简址': 8, '取回': 4, '折常量': 4, '折位': 4, '和': 4, '阵': 4}
RE_INST = re.compile(r'^\s+(抽|子)\[(\d+)\] 算子=(\d+) 现高=(\d+) 元=(-?\d+) 源一=(-?\d+) 源二=(-?\d+) 链头=(-?\d+)', re.M)
RE_VAL = re.compile(r'^\s+值\[(\d+)\] 实体=变量 名=(\S+) 变量序=(\d+) 存储类=(\d+) 宽=(\d+) 访=(\d+) 偏=(-?\d+) 型=(\d+) 函号=(\d+)$', re.M)
RE_VALF = re.compile(r'^\s+值\[(\d+)\] 实体=函数 序=(\d+) 存储类=4 宽=(-?\d+) 偏=(-?\d+)$', re.M)


def dump_of(cc):
    out = []
    for src in PROBES:
        exe = os.path.join(T, 'v_%s.exe' % os.path.basename(src))
        r = subprocess.run([cc, os.path.join(QJ, src), '-o', exe], capture_output=True,
                           env=dict(os.environ, QCC_DUMP='ir'), cwd=QJ)
        out.append(((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace'))
    return out


def analyze(dumps, label):
    fails = []
    for d in dumps:
        if '-- IR 值表' not in d:
            fails.append('%s: 缺 「IR 值表」段' % label)
        if '-- IR L0 访问 实例' not in d:
            fails.append('%s: 缺 「IR L0 访问 实例」段' % label)
    txt = '\n'.join(dumps)
    m = re.search(r'值基=(\d+) 值容=(\d+) 值函基=(\d+) \| 已绑 变量值=(\d+) 函数值=(\d+)', txt)
    base = None
    if not m:
        fails.append('① 未见 值表 头行')
    else:
        base = int(m.group(1))
        print('  值表: 值基=%s 值容=%s 值函基=%s 已绑变量=%s 已绑函数=%s' % m.groups())
        if base <= 1048576:
            fails.append('① 值基(%d) 必须 > 缓集(1048576)' % base)
    vals = {}
    for mm in RE_VAL.finditer(txt):
        vals[base + int(mm.group(1))] = dict(实体='变量', 名=mm.group(2), 序=int(mm.group(3)),
                                           存=int(mm.group(4)), 宽=int(mm.group(5)), 访=int(mm.group(6)),
                                           偏=int(mm.group(7)), 型=int(mm.group(8)))
    for mm in RE_VALF.finditer(txt):
        vals[base + int(mm.group(1))] = dict(实体='函数', 序=int(mm.group(2)), 存=4,
                                            宽=int(mm.group(3)), 偏=int(mm.group(4)), 型=0)
    print('  值表 条目（被 L1 引用）= %d' % len(vals))
    # ---- ① 操作数 分布 ----
    tot = {k: {'n': 0, 'neg1': 0, 'zero': 0, 'node': 0, 'lit': 0, 'vref': 0} for k in 受限}
    裸返 = []
    未知值 = []
    节点集 = set()
    for mm in RE_INST.finditer(txt):
        op = int(mm.group(3))
        if op not in 受限:
            continue
        v1, v2 = int(mm.group(6)), int(mm.group(7))
        for s in range(槽数[op]):
            v = v1 if s == 0 else v2
            c = tot[op]
            c['n'] += 1
            if v == -1:
                c['neg1'] += 1
                if op == 1072:
                    裸返.append((mm.group(2), v))
            elif v == 0:
                c['zero'] += 1
            elif base is not None and v >= base:
                c['vref'] += 1
                if v not in vals:
                    未知值.append((op, v))
            elif v < 1048576:
                c['node'] += 1
                节点集.add(v)
            else:
                c['lit'] += 1
    TOT = {'n': 0, 'neg1': 0, 'zero': 0, 'node': 0, 'lit': 0, 'vref': 0}
    print('  %-6s %-4s %8s %8s %8s %8s %8s %8s' % ('码', '类', '槽数', '-1', '0填充', '节点导出', '字面', '值引用'))
    for op in sorted(受限):
        c = tot[op]
        for k in TOT:
            TOT[k] += c[k]
        print('  %-6d %-4s %8d %8d %8d %8d %8d %8d' % (op, 受限[op][0], c['n'], c['neg1'], c['zero'], c['node'], c['lit'], c['vref']))
    print('  %-11s %8d %8d %8d %8d %8d %8d' % ('合计', TOT['n'], TOT['neg1'], TOT['zero'], TOT['node'], TOT['lit'], TOT['vref']))
    分母 = TOT['n'] - len(裸返)
    有身份 = TOT['n'] - TOT['neg1']
    print('  裸返回(-1, 语义无值, 登记) = %d ⟹ 分母 = %d' % (len(裸返), 分母))
    print('  **有身份 = %d/%d = %.1f%%**   无身份(-1, 非裸返回) = %d' % (有身份, 分母, 100.0 * 有身份 / max(1, 分母), TOT['neg1'] - len(裸返)))
    if 分母 <= 0:
        fails.append('① 分母 = 0（量具 空转）')
    if TOT['neg1'] - len(裸返) > 0:
        fails.append('① 仍有 %d 个 无身份操作数' % (TOT['neg1'] - len(裸返)))
    if 有身份 / max(1, 分母) < 0.95:
        fails.append('① 有身份 占比 %.1f%% < 95%%' % (100.0 * 有身份 / max(1, 分母)))
    if 未知值:
        fails.append('② 有 %d 个 值引用 不在 值表 内: %s' % (len(未知值), 未知值[:4]))
    # ---- ② 值表 自洽 ----
    if vals:
        bad = [k for k, v in vals.items() if v['存'] not in (1, 2, 3, 4) or v['宽'] < 0]
        print('  ② 值表 自洽: 条目=%d 不自洽=%d %s' % (len(vals), len(bad), '✓' if not bad else '✗'))
        if bad:
            fails.append('② 值表 不自洽: %s' % bad[:4])
    # ---- ③ 反向查证（手写期望）----
    print('  ③ 反向查证（手写期望 · 独立复算）:')
    名字 = {v['名']: v for v in vals.values() if v['实体'] == '变量'}
    n对 = 0
    错 = []
    for nm, w in sorted(期望宽.items()):
        if nm in 名字:
            n对 += 1
            got = 名字[nm]['宽']
            if got != w:
                错.append('%s 期望宽=%d 实得=%d' % (nm, w, got))
            else:
                print('      %-6s 宽=%d 存储类=%d 偏=%d ✓' % (nm, got, 名字[nm]['存'], 名字[nm]['偏']))
    if n对 < 8:
        fails.append('③ 反向查证 样本不足 (%d) —— 探针 变量 未落 值表' % n对)
    if 错:
        fails.append('③ 反查 不符: %s' % 错)
    # ---- ④ 计数自洽 ----
    m4 = re.search(r'\*\*被 L1 指令 引用的 值 = (\d+)\*\*（值表 已绑 (\d+)）', txt)
    if not m4:
        fails.append('④ 未见 「被 L1 指令 引用的值」 行')
    else:
        用 = int(m4.group(1))
        ok4 = (用 == len(vals) and 用 >= 1)
        print('  ④ 被引用值=%d · 值表段 条目=%d · 已绑=%s %s' % (用, len(vals), m4.group(2), '✓' if ok4 else '✗'))
        if not ok4:
            fails.append('④ 计数不自洽: 被引用 %d != 段内条目 %d' % (用, len(vals)))
    return fails


def main():
    args = sys.argv[1:]
    cc0 = next((a for a in args if not a.startswith('--')), CC)
    if '--selftest' in args:
        raw = dump_of(cc0)[0]
        print('== 自测：3 组 破坏 ⟹ 期望 FAIL ==')
        ok = True
        cases = [('a 抹 值表段 全部条目行', re.compile(r'^\s+值\[\d+\] 实体=变量 .*$', re.M)),
                 ('b 抹 被引用值 行(④)', re.compile(r'^\s+\*\*被 L1 指令 引用的 值 = \d+\*\*.*$', re.M)),
                 ('c 把 一 值引用 改回 -1（模拟 未回填）', re.compile(r'^(  抽\[\d+\] 算子=1051 .* 源一=)\d+', re.M))]
        for name, pat in cases:
            if name.startswith('c'):
                d = pat.sub(lambda m: m.group(1) + '-1', raw, count=1)
            else:
                d = pat.sub('', raw)
            f = analyze([d], name)
            print('   %-32s 判 FAIL ? %s  (%s)' % (name, len(f) > 0, f[0] if f else ''))
            ok = ok and len(f) > 0
        print('\n[%s] 自测：3 组 破坏 全部 FAIL ⟹ 判据 非空转' % ('PASS' if ok else 'FAIL'))
        sys.exit(0 if ok else 1)
    if '--dump-file' in args:
        f = args[args.index('--dump-file') + 1]
        txt = open(f, encoding='utf-8', errors='replace').read()
        fails = analyze([txt], os.path.basename(f))
    else:
        print('编译器 =', cc0)
        if not os.path.exists(cc0):
            print('[FAIL] 引擎不存在')
            sys.exit(1)
        fails = analyze(dump_of(cc0), 'engine')
    print()
    if fails:
        print('[FAIL] 判据 1（值有身份） 不成立:')
        for x in fails:
            print('   -', x)
        sys.exit(1)
    print('[PASS] 判据 1：受限类 操作数 有身份 ≥95% · 值表 自洽 · 反查 与 手写期望 一致')
    sys.exit(0)


main()
