# -*- coding: utf-8 -*-
"""qiyuan_ledger_terminal.py -- 台账 **终态 逐行核**（plan 验证步骤 2 的 机器 实现）。

用法: python scripts\\qiyuan_ledger_terminal.py <SCRATCH目录>
输出: {SCRATCH}/ledger_terminal.log

检查（每条 = 23 行 + GLOBAL）:
  A. 状态 非空 且 ∈ {已解决, 部分, 未解决, 作废}
  B. **已解决** 条: 事实格 必须 **全部 是 `key=value`**（非空、非 `-`）——「无证据条目」的 机器 定义
  C. **未解决 / 部分** 条: 复算列 必须 含 ≥1 个 **在仓**（`git ls-files` 可见）的 生成器/最小复现 路径
观察（plan 要求 出现 三项）: `待核=0` · `无证据条目=0` · `未在仓最小复现=0`
"""
import io, os, re, subprocess, sys

QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
P = os.path.join(QJ, 'scripts', 'qiyuan_ledger_facts.tsv')
S = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('QCC_LEDGER_SCRATCH', '')
if not S or not os.path.isdir(S):
    sys.exit('用法: python scripts\\qiyuan_ledger_terminal.py <SCRATCH目录>')
S = os.path.abspath(S)
LEGAL = ('已解决', '部分', '未解决', '作废')
PATHRE = re.compile(r'([\w\\/\.\-]+\.(?:py|c|ps1|tsv|md))')


def in_repo(p):
    """返回 **仓内** 路径（`git ls-files` 可见），否则 None。

    候选 = 原样 / `scripts/` 前缀 / `scripts/probe/` 前缀 —— 因为 复算列 里 既可能 写
    `scripts\\x.py`，也可能 只 写 裸名 `x.py`。
    ⚠️ gap1 教训：`.gitignore` 的 `_*.py` 会 让 看似 存在的 脚本 **不在仓** ⟹ 必须 用
    `git ls-files --error-unmatch` 实测，而不是 `os.path.exists`。
    """
    base = os.path.basename(p)
    for c in (p, 'scripts/' + base, 'scripts/probe/' + base):
        q = c.replace('\\', '/')
        r = subprocess.run(['git', 'ls-files', '--error-unmatch', q], capture_output=True, cwd=QJ)
        if r.returncode == 0:
            return q
    return None


rows, glob = [], None
for line in io.open(P, encoding='utf-8').read().split('\n'):
    if not line.strip() or line.startswith('#'):
        continue
    c = line.split('\t')
    if len(c) < 7:
        continue
    rec = dict(zip(['id', '递刀', '性质', '状态', '证据', '复算', '事实'], c))
    (glob := rec) if rec['id'] == 'GLOBAL' else rows.append(rec)

L, bad = [], []
L.append('# 台账 终态 逐行核（plan 验证步骤 2）· 事实源 = scripts/qiyuan_ledger_facts.tsv')
L.append('')
L.append('| id | 状态 | 事实格 | 事实 key=value | 仓内最小复现 |')
L.append('|:--|:--|:--|:--|:--|')

n_todo = n_noev = n_norepo = 0
for r in rows + [glob]:
    st = r['状态'].strip()
    fcells = [x.strip() for x in r['事实'].split(';') if x.strip() and x.strip() != '-']
    kv_ok = bool(fcells) and all(('=' in x and len(x.split('=', 1)[0].strip()) > 0) for x in fcells)
    if st not in LEGAL or not st:
        n_todo += 1
        bad.append('id=%s 状态 非法/空: %r' % (r['id'], st))
    if st == '已解决' and not kv_ok:
        n_noev += 1
        bad.append('id=%s 已解决 但 事实格 非 key=value: %r' % (r['id'], r['事实']))
    paths = [m.group(1) for m in PATHRE.finditer(r['复算'])]
    repo = [q for q in (in_repo(p) for p in paths) if q]
    # gap2 修 (2026-10-14): 「在仓 复算/最小复现」判据 覆盖 **全部行** —— 原先 `nopath_needed`
    #   只覆盖 未解决/部分 ⟹ **已解决**行 的 复算列 即便 指向 不在仓 的脚本（如 被 .gitignore 命中的
    #   `_*.py`），也 **不被 任何 校验器 捕捉** ⟹ 判据 4 的 空洞 由此 堵上。
    ok_repo = bool(repo)
    if not ok_repo:
        n_norepo += 1
        bad.append('id=%s 复算列 无 **在仓** 生成器/最小复现: %r' % (r['id'], r['复算'][:70]))
    L.append('| %s | %s | %s | %s | %s |' % (
        r['id'], st, (r['事实'][:52] + ('…' if len(r['事实']) > 52 else '')),
        '✅' if kv_ok else '—', (', '.join(repo) if repo else '❌')))

L.append('')
L.append('## 观察（plan 要求 三项 必须 出现）')
L.append('- `待核=0`  ⟹ 实测 **待核=%d**（状态 非法/空 计数；`待核` 字面 在 本表 已 不存在）' % n_todo)
L.append('- `无证据条目=0` ⟹ 实测 **无证据条目=%d**（已解决 但 事实格 非 key=value）' % n_noev)
L.append('- `未在仓最小复现=0` ⟹ 实测 **未在仓最小复现=%d**（**全部 24 行** 的 复算列 里 找不到 **在仓**（`git ls-files` 可见）路径 的 条数）' % n_norepo)
L.append('')
cnt = {}
for r in rows:
    cnt[r['状态']] = cnt.get(r['状态'], 0) + 1
L.append('## 派生 计数（从 状态列 数 · 不读 任何 文档）')
L.append('- 合计 %d · ' % len(rows) + ' · '.join('%s %d' % (k, cnt[k]) for k in sorted(cnt)))
L.append('')
L.append('判定 = ' + ('✅ 终态 逐行核 全过' if not bad else '❌ ' + ' | '.join(bad[:8])))
io.open(os.path.join(S, 'ledger_terminal.log'), 'w', encoding='utf-8', newline='').write('\n'.join(L) + '\n')
print('\n'.join(L[-6:]))
print('待核=%d 无证据条目=%d 未在仓最小复现=%d' % (n_todo, n_noev, n_norepo))
sys.exit(0 if not bad else 1)
