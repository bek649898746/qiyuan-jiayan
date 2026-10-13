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
PATHRE = re.compile(r'(scripts[\\/][\w\\/\.\-]+\.(?:py|c|ps1|tsv|md)|probe[\\/][\w\.]+\.c)')


def in_repo(p):
    q = p.replace('\\', '/')
    r = subprocess.run(['git', 'ls-files', '--', q], capture_output=True, cwd=QJ)
    return bool((r.stdout or b'').strip())


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
    paths = [m.group(1) for m in PATHRE.finditer(r['复算'])]
    repo = [p for p in paths if in_repo(p)]
    nopath_needed = st in ('未解决', '部分')
    ok_repo = bool(repo) or not nopath_needed
    if st not in LEGAL or not st:
        n_todo += 1; bad.append('id=%s 状态 非法/空: %r' % (r['id'], st))
    if st == '已解决' and not kv_ok:
        n_noev += 1; bad.append('id=%s 已解决 但 事实格 非 key=value: %r' % (r['id'], r['事实']))
    if nopath_needed and not repo:
        n_norepo += 1; bad.append('id=%s %s 但 复算列 无 在仓 最小复现: %r' % (r['id'], st, r['复算'][:70]))
    L.append('| %s | %s | %s | %s | %s |' % (
        r['id'], st, (r['事实'][:52] + ('…' if len(r['事实']) > 52 else '')),
        '✅' if kv_ok else '—', (', '.join(repo) if repo else ('—' if not nopath_needed else '❌'))))

L.append('')
L.append('## 观察（plan 要求 三项 必须 出现）')
L.append('- `待核=0`  ⟹ 实测 **待核=%d**（状态 非法/空 计数；`待核` 字面 在 本表 已 不存在）' % n_todo)
L.append('- `无证据条目=0` ⟹ 实测 **无证据条目=%d**（已解决 但 事实格 非 key=value）' % n_noev)
L.append('- `未在仓最小复现=0` ⟹ 实测 **未在仓最小复现=%d**（未解决/部分 但 复算列 无 在仓 路径）' % n_norepo)
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
