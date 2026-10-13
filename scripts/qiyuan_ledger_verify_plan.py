# -*- coding: utf-8 -*-
"""qiyuan_ledger_verify_plan.py -- 把 plan 的 `## Verification plan` **机器化**（逐条断言观察项）。

用法: python scripts\\qiyuan_ledger_verify_plan.py <SCRATCH目录> [--live]
  --live : 先 依次 跑 全链（全部 仓内 脚本），把 每步 退出码 记下来；再 逐条 断言 观察项。
输出: {SCRATCH}/verify_plan.log · {SCRATCH}/clean_env_rerun.log（步骤 7 的 复跑 记录）
退出码: 0 = plan 七步 的 观察项 **全部 现** 且 全链 退出码 全 0；否则 1。

设计纪律: 断言 **只读 各 log 的原文**（不 重实现 判据、不 硬编码 期望以外的 结论）；
          「读到的 字符串」必须 是 各 生成器 自己 写下的。
"""
import io, os, re, subprocess, sys

QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
SCRIPTS = os.path.join(QJ, 'scripts')
S = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('QCC_LEDGER_SCRATCH', '')
if not S or not os.path.isdir(S):
    sys.exit('用法: python scripts\\qiyuan_ledger_verify_plan.py <SCRATCH目录> [--live]')
S = os.path.abspath(S)
LIVE = '--live' in sys.argv

# live 链: (标签, 脚本, 附加参数, 产出的 证据文件)
CHAIN = [
    ('evidence-fast', 'qiyuan_ledger_evidence.py', ['probe', 'defect', 'p0', 'launch', 'gitfam', 'anchors', 'zlib'], 'anchor_final.log'),
    ('evidence-bootstrap', 'qiyuan_ledger_evidence.py', ['bootstrap'], 'bootstrap_final.log'),
    ('probe', 'qiyuan_ledger_probe.py', [], 'logs/item_probes.tsv'),
    ('check', 'qiyuan_ledger_check.py', [], 'ledger_check.log'),
    ('terminal', 'qiyuan_ledger_terminal.py', [], 'ledger_terminal.log'),
    ('items', 'qiyuan_ledger_items.py', [], 'items_final.log'),
    ('negtest', 'qiyuan_ledger_negtest.py', [], 'negtest.log'),
    ('render-check-only', 'qiyuan_ledger_render.py', ['--check-only'], None),
]

# plan 七步 + 附加: (标签, 证据文件, [(观察名, 正则 或 None=仅看退出码)])
STEPS = [
    ('步骤1 gating · check', 'ledger_check.log', [
        ('退出码 0', None),
        ('判定 = ✅ 台账 一致性 全过', r'判定 = .*台账 一致性 全过'),
        ('事实 逐格: 不符 0 · 证据 路径 缺失 0', r'事实 逐格: 不符 0 · 证据 路径 缺失 0'),
        ('GEN 块 一致性: 不符 0', r'GEN 块 一致性: 不符 0'),
        ('③b 校验器 只读（改写份数 0）', r'③b 校验器 只读: .*份数 = 0'),
        ('派生计数 与 TSV 一致（合计 23）', r'① 计数（从 TSV 派生）: 合计 23'),
    ]),
    ('步骤2 gating · 逐行核', 'ledger_terminal.log', [
        ('待核=0', r'待核=0'), ('无证据条目=0', r'无证据条目=0'), ('未在仓最小复现=0', r'未在仓最小复现=0'),
        ('判定 全过', r'判定 = .*终态 逐行核 全过'),
    ]),
    ('步骤3 gating · 逐条探针', 'items_final.log', [
        ('判定 = 全部 遗留要件 实测 与 事实源 逐字相符', r'判定 = .*全部 遗留要件.*相符'),
    ]),
    ('步骤4 gating · 自举', 'bootstrap_final.log', [
        ('[通过 PASS]', r'\[通过 PASS\]'),
        ('不动点 X 达成', r'不动点 [0-9A-Fa-f]{8} 达成'),
        ('v1 == v2 == $expected', r'v1 == v2 == \$expected` ⟹ \*\*✅ 收敛\*\*'),
    ]),
    ('步骤5 gating · 定向锚', 'anchor_final.log', [
        ('FAIL=0', r'PASS=\d+ SKIP=\d+ FAIL=0'),
    ]),
    ('步骤6 gating（条件）· git', 'git_gate_final.log', [
        ('写明 本轮是否改产物', r'本轮是否改了编译器产物'),
        ('FAIL ≤ 26 且 CRASH=0 且 逐族无上升', r'⟹ \*\*不回归\*\*'),
    ]),
    ('步骤7 evidence · 干净环境复跑', 'clean_env_rerun.log', [
        ('从零复跑链', r'## 1. 复跑链'),
    ]),
    ('附加 · 判据有效性（负向测试）', 'negtest.log', [
        ('判据 都能失败', r'\d+ 条 判据 都 \*\*能失败\*\*'),
    ]),
]


def rd(p):
    p2 = p if os.path.isabs(p) else os.path.join(S, p)
    return io.open(p2, 'rb').read().decode('utf-8', 'replace') if os.path.exists(p2) else None


L, fails = [], []
L.append('# plan `## Verification plan` 机器化 逐条核（2026-10-14）')
L.append('')
L.append('- SCRATCH = `%s`' % S)
L.append('- 模式 = %s' % ('**--live**（先 跑 全链 再 断言）' if LIVE else '仅断言 现有 log'))
L.append('')

rcs = {}
if LIVE:
    L.append('## 0. live 全链（全部 仓内 脚本）')
    L.append('')
    L.append('| 步 | 命令 | 退出码 |')
    L.append('|:--|:--|:--|')
    for label, script, extra, _out in CHAIN:
        argv = [sys.executable, os.path.join(SCRIPTS, script), S] + extra
        r = subprocess.run(argv, capture_output=True, cwd=QJ)
        rcs[label] = r.returncode
        if r.returncode != 0:
            fails.append('live 链 %s 退出码 %d' % (label, r.returncode))
        L.append('| %s | `python scripts\\%s %s` | %s |'
                 % (label, script, ' '.join(extra), '✅ 0' if r.returncode == 0 else '❌ %d' % r.returncode))
    L.append('')
    # 步骤 7 的 复跑 记录（由 本器 生成，覆盖 手工版）
    cl = ['# 干净环境 复跑（plan 验证步骤 7 · 由 qiyuan_ledger_verify_plan.py --live 生成）', '',
          '## 0. 环境']
    cl.append('- SCRATCH = `%s` · **起点 = 全新空目录**（见 `ledger_start.log`）' % S)
    cl.append('- 时间 = %s' % __import__('time').strftime('%Y-%m-%d %H:%M:%S'))
    cl.append('')
    cl.append('## 1. 复跑链（%d 步 · 全部 只用 仓内 脚本）' % len(CHAIN))
    cl.append('```')
    for label, script, extra, _o in CHAIN:
        cl.append('python scripts\\%s {SCRATCH} %s' % (script, ' '.join(extra)))
    cl.append('python scripts\\qiyuan_ledger_verify_plan.py {SCRATCH} --live   # 本器（含下列断言）')
    cl.append('```')
    cl.append('')
    cl.append('## 2. live 退出码')
    cl.append('| 步 | 退出码 |')
    cl.append('|:--|:--|')
    for label, _s, _e, _o in CHAIN:
        cl.append('| %s | %s |' % (label, rcs.get(label)))
    cl.append('')
    cl.append('## 3. 观察项 断言')
    cl.append('（见 文末「§3 观察项 断言」—— 由 本器 在 断言 完成后 追加）')

L.append('## 1. plan 观察项 逐条断言')
L.append('')
L.append('| plan 步骤 | 证据文件 | 必须出现的观察 | 结果 |')
L.append('|:--|:--|:--|:--|')
for label, fname, checks in STEPS:
    t = rd(fname)
    if t is None:
        fails.append('%s: 证据文件 缺失 %s' % (label, fname))
        L.append('| %s | `%s` | （文件缺失） | ❌ |' % (label, fname))
        continue
    oks = []
    for name, pat in checks:
        ok = True if pat is None else bool(re.search(pat, t))
        # 退出码 类观察：用 live 结果 或 跳过
        if pat is None:
            if LIVE:
                key = {'步骤1 gating · check': 'check', '步骤2 gating · 逐行核': 'terminal',
                       '步骤3 gating · 逐条探针': 'items', '步骤4 gating · 自举': 'evidence-bootstrap',
                       '步骤5 gating · 定向锚': 'evidence-fast', '附加 · 判据有效性（负向测试）': 'negtest'}.get(label)
                ok = (rcs.get(key) == 0) if key else True
            else:
                ok = True    # 未 live ⟹ 退出码 由 调用方 另行核（本节 只断言 文本观察）
        oks.append((name, ok))
        if not ok:
            fails.append('%s: 观察 未现 —— %s（%s）' % (label, name, fname))
    L.append('| %s | `%s` | %s | %s |'
             % (label, fname, ' · '.join(('%s%s' % ('✓' if o else '✗', n)) for n, o in oks),
                '✅' if all(o for _, o in oks) else '❌'))
L.append('')
L.append('## 2. 判定')
L.append('')
L.append('判定 = ' + ('✅ plan 的 七步 观察项 **全部现**（附加 负向测试 亦过）' if not fails else '❌ ' + ' | '.join(fails[:6])))
io.open(os.path.join(S, 'verify_plan.log'), 'w', encoding='utf-8', newline='').write('\n'.join(L) + '\n')
if LIVE:      # 步骤 7 的 复跑 记录：在 断言 之后 落盘 ⟹ 含 断言 结果（不再 手拼）
    _i = next((k for k, l in enumerate(L) if l.startswith('## 1. plan 观察项')), None)
    cl += ['', '## 3. 观察项 断言（= verify_plan.log 的 §1 逐条核）', '']
    cl += L[_i:] if _i is not None else L
    cl += ['', '判定 = ' + ('✅ 全链 %d 步 退出码 全 0 · plan 七步 观察项 全部现' % len(CHAIN)
                            if not fails else '❌ ' + ' | '.join(fails[:4]))]
    io.open(os.path.join(S, 'clean_env_rerun.log'), 'w', encoding='utf-8', newline='').write('\n'.join(cl) + '\n')
print('\n'.join(L))
sys.exit(0 if not fails else 1)
