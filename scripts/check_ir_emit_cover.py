# -*- coding: utf-8 -*-
"""刀九 判据检查器：**L1 发射覆盖 收口**（plan 验收判据 1）。

判据:
  ① 七类代表形态 **各 ≥1 条真实发射**（主槽 `抽[]` 或 子链 `子[]` 均算）:
     位运算(1021–1026) · 浮点↔整数转换(1064–1066) · 调用(1071) · 返回(1072) ·
     跳(1041)/条件跳(1042) · 局部栈对象(1081) · 取址(1102)
  ② 四类**必发**：位运算 · 条件跳 · 调用 · 返回 —— 缺一 ⟹ FAIL
  ③ 零发射项（码表「项码」中 dump 里 0 条者）**逐条 有登记理由**（本文件 `登记` 表）；
     且 零发射项数 **< 基线**（刀九 前 实测 = 15）
④ 计数自洽（**弱式 · 实测量具口径**）：`现序一` 是**写口计数**（主槽 首写 + 子链 每次追加），
     dump 是**存活记录**（每节点一条；子链 走 `链头`）。⚠️ 已知：节点被**再次** `发抽象` 时
     `剩十 = -1` 会**弃链**（旧子链成孤儿 ⟹ 不打印但 已 计数）⟹ 严格等式**不成立**（本刀前后
     实测 缺口 +2/+3/+4 → +13/+14/+15）。故判据 = `现序一 >= 记录数`（防少记）+ 记录数 > 0，
     并把缺口**如实打印**（量具口径，供审计复核）。
用法:
  python scripts/check_ir_emit_cover.py [引擎]
  python scripts/check_ir_emit_cover.py --dump-file <f>
  python scripts/check_ir_emit_cover.py --selftest      # 4 组 破坏 必须 FAIL（判据不自空转）
"""
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
T = os.path.join(os.environ.get('TEMP', '.'), 'qiyuan-ir-cover')
os.makedirs(T, exist_ok=True)
CC = os.path.join(QJ, 'qcc_zh_v2.exe')

PROBES = ['scripts/probe/ir_cover_probe.c', 'scripts/probe/ir_cfg_probe.c', 'scripts/probe/ir_probe.c']

# ---- ③ 零发射项 登记表（逐条 理由 + 触发条件；**禁中间态、禁静默丢弃**）----
登记 = {
    1066: ('浮转浮 fp2fp', '本轮 未落 L1 发射点',
           'L0 侧 已有 (栈丁单/内酉单/名进单/前八一单 = 4B `单` 字段/局部 的 movss+cvt)；'
           'L1 侧 落点 未定 —— 「4B 单精度 读 → 提升 双精度」是 `码生成浮()` 内 的**隐式**换算，'
           '且 与 1064/1065 不同，它 不 经过 明确 的 「整↔浮」边界 ⟹ 须先定「单精度 值 在 IR 里 怎么命名」。'
           '触发条件: 与 变量值命名 同批落地 后 单点补。'),
    1121: ('不可达 unreachable', '本轮 未落 L1 发射点（**设计使然**）',
           'C 语言 无「不可达」语句形态（对照表 表B G6 登记: 为 后续 pass 预留）；'
           '触发条件: 常量折叠 / 分支剪枝 pass 落地 后，由 pass 自身 发射（本层 不 硬塞）。'),
}
基线零发射 = 14   # 刀九 前 实测（复现 plan 快照「起点 = 14」）: 1021–1026 · 1064 · 1065 · 1066 · 1071 · 1072 · 1081 · 1101 · 1121

类覆盖 = [
    ('位运算', [1021, 1022, 1023, 1024, 1025, 1026]),
    ('浮点↔整数转换', [1064, 1065, 1066]),
    ('调用', [1071]),
    ('返回', [1072]),
    ('跳/条件跳', [1041, 1042]),
    ('局部栈对象', [1081]),
    ('取址', [1102]),
]
必发类码 = {'位运算': [1021, 1022, 1023, 1024, 1025, 1026], '跳/条件跳': [1042],
            '调用': [1071], '返回': [1072]}

RE_DEF = re.compile(r'^\s+(\d{4})\s+(\S+)\s*/\s*(\S+)\s*$', re.M)
RE_INST = re.compile(r'^\s+(?:抽|子)\[\d+\] 算子=(\d+)', re.M)
RE_XIAN = re.compile(r'L1 抽象指令 .*?现序一=(\d+)')


def dump_of(cc):
    out = []
    for src in PROBES:
        p = os.path.join(QJ, src)
        exe = os.path.join(T, 'cover_%s.exe' % os.path.basename(src))
        r = subprocess.run([cc, p, '-o', exe], capture_output=True,
                           env=dict(os.environ, QCC_DUMP='ir'), cwd=QJ)
        out.append((os.path.basename(src),
                    ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')))
    return out


def analyze(dumps):
    """dumps = [(label, text)]；返回 (fails, cnt, defs)。"""
    fails = []
    defs = {}
    cnt = {}
    for label, txt in dumps:
        for c, cn, en in RE_DEF.findall(txt):
            c = int(c)
            if 1000 <= c < 2000 and c % 10 != 0:      # 「项码」= 尾非 0（尾 0 是 12 类 类码）
                defs[c] = (cn, en)
        n_main = len(re.findall(r'^\s+抽\[\d+\] 算子=', txt, re.M))
        n_sub = len(re.findall(r'^\s+子\[\d+\] 算子=', txt, re.M))
        for m in RE_INST.finditer(txt):
            cnt[int(m.group(1))] = cnt.get(int(m.group(1)), 0) + 1
        x = RE_XIAN.findall(txt)
        if not x:
            fails.append('%s: 未见 L1 抽象指令 段' % label)
        else:
            gap = int(x[0]) - (n_main + n_sub)
            print('    ④ %-22s 现序一=%-6s 主槽=%-6d 子链=%-4d 缺口=%+d  %s'
                  % (label, x[0], n_main, n_sub, gap, '✓' if gap >= 0 else '✗ 少记'))
            if gap < 0:
                fails.append('④ %s: 现序一(%s) < 主槽(%d)+子链(%d) ⟹ 有记录 未计数' % (label, x[0], n_main, n_sub))
            if n_main + n_sub <= 0:
                fails.append('④ %s: 记录数 = 0' % label)
    if not defs:
        fails.append('未见 码表（dump 缺段）')
    return fails, cnt, defs


def report(fails, cnt, defs, title):
    print('  [%s] 码表 项码=%d · 实测 命中 项码=%d · 零发射=%d'
          % (title, len(defs), len([c for c in cnt if c in defs]), len([c for c in defs if cnt.get(c, 0) == 0])))
    for cls, codes in 类覆盖:
        got = sum(cnt.get(c, 0) for c in codes)
        print('    %-14s %s  (%s)' % (cls, '✓' if got >= 1 else '✗',
                                      ' '.join('%d:%d' % (c, cnt.get(c, 0)) for c in codes)))
        if got < 1:
            fails.append('① 类缺失: %s' % cls)
    for cls, codes in 必发类码.items():
        if sum(cnt.get(c, 0) for c in codes) < 1:
            fails.append('② 必发类缺失: %s' % cls)
    zero = sorted(c for c in defs if cnt.get(c, 0) == 0)
    print('    零发射项 %d 个: %s' % (len(zero), zero))
    for c in zero:
        if c not in 登记:
            fails.append('③ 零发射项 未登记: %d (%s)' % (c, defs[c][0]))
        else:
            print('      %d %s ⟹ 已登记: %s' % (c, 登记[c][0], 登记[c][1]))
    mark = '✓ 下降' if len(zero) < 基线零发射 else '✗ 未下降'
    print('    基线 零发射 = %d ⟹ 实测 %d  %s' % (基线零发射, len(zero), mark))
    if len(zero) >= 基线零发射:
        fails.append('③ 零发射项数 %d 未低于基线 %d' % (len(zero), 基线零发射))
    return fails


def main():
    args = sys.argv[1:]
    cc0 = next((a for a in args if not a.startswith('--')), CC)
    if '--selftest' in args:
        base = dump_of(cc0)
        raw = base[0][1]        # ★ 只用 覆盖探针 那一次 dump：④ 自洽 仍成立 ⟹ 失败 只 可能 来自 ①②③
        print('== 自测：对 覆盖探针 dump 做 4 组 破坏 ⟹ 期望 全部 FAIL ==')
        ok = True
        cases = [('a 抹 位运算(1021-1026)', r'^\s+(?:抽|子)\[\d+\] 算子=102[1-6] .*$'),
                 ('b 抹 调用/返回(1071/1072)', r'^\s+(?:抽|子)\[\d+\] 算子=107[12] .*$'),
                 ('c 抹 取址(1102)', r'^\s+(?:抽|子)\[\d+\] 算子=1102 .*$'),
                 ('d 抹 局部栈对象(1081)', r'^\s+(?:抽|子)\[\d+\] 算子=1081 .*$')]
        for name, pat in cases:
            doctored = re.sub(pat, '', raw, flags=re.M)
            _f, _c, _d = analyze([(name, doctored)])
            f = report(_f, _c, _d, name)
            print('   %-26s 判 FAIL ? %s' % (name, len(f) > 0))
            ok = ok and len(f) > 0
        print('\n[%s] 自测：4 组 破坏 全部 FAIL ⟹ 判据 非空转' % ('PASS' if ok else 'FAIL'))
        sys.exit(0 if ok else 1)
    if '--dump-file' in args:
        f = args[args.index('--dump-file') + 1]
        txt = open(f, encoding='utf-8', errors='replace').read()
        fails, cnt, defs = analyze([(os.path.basename(f), txt)])
        fails = report(fails, cnt, defs, os.path.basename(f))
    else:
        cc = args[0] if args and not args[0].startswith('--') else CC
        print('编译器 =', cc)
        if not os.path.exists(cc):
            print('[FAIL] 引擎不存在: %s' % cc)
            sys.exit(1)
        fails, cnt, defs = analyze(dump_of(cc))
        fails = report(fails, cnt, defs, 'engine')
    print()
    if fails:
        print('[FAIL] 刀九 发射覆盖 判据 不成立:')
        for x in fails:
            print('   -', x)
        sys.exit(1)
    print('[PASS] 刀九 发射覆盖：七类 各 ≥1 · 四类必发齐 · 零发射项 全部有登记 且 < 基线')
    sys.exit(0)


main()
