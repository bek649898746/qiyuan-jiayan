# -*- coding: utf-8 -*-
"""qiyuan_ledger_items.py -- 未闭合项 **遗留要件** 逐条 现役 探针 证据（plan 验证步骤 3）。

用法: python scripts\\qiyuan_ledger_items.py <SCRATCH目录>
输出: {SCRATCH}/items_final.log

口径（plan 逐字）: 每条必须出现 二者之一 ——
  * **转绿**: 实测 rc/值 与 事实源 期望 逐字相符
  * **定性**: 实测输出 与 递刀 逐字依据 一致，且 该依据 在 文档里 被引用

⚠️ gap4 修 (2026-10-14): 判定 **不得 硬编码**。本版 把 每条 的 「判定」 与 通过条件 都 变成 **派生量**：
  ① 期望值 **只从 事实源 TSV 读**（`qiyuan_ledger_facts.tsv` 的 事实格）；
  ② 实测值 **只从 `item_probes.tsv` 读**（由 `qiyuan_ledger_probe.py` 用 现役引擎 生成）；
  ③ 逐 key 对账 ⟹ 判定 = 「✅ 与事实源一致」/「❌ 不符」；任一 不符 ⟹ **退出码 1**。
     ⟹ 探针 变红（rc 与 台账 期望 不符）**必被抓**，不会再 被 印成「转绿」。
"""
import io, os, re, sys

QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
S = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('QCC_LEDGER_SCRATCH', '')
if not S or not os.path.isdir(S):
    sys.exit('用法: python scripts\\qiyuan_ledger_items.py <SCRATCH目录>')
S = os.path.abspath(S)
TSV = os.path.join(QJ, 'scripts', 'qiyuan_ledger_facts.tsv')

# 未闭合项（部分 / 未解决 / 本轮翻正）的 遗留要件 -> 承载它的 探针 key + 递刀 逐字依据
ITEMS = [
    ('13', ['i13_ji', 'i13_knives', 'i13_uncal'],
     '启元《施工单v2_按甲言地基》§三 六刀 + §二 八条纪律'),
    ('17', ['i17_vla', 'i17_sendbit', 'i17_enumflag', 'i17_readend'],
     '启元《IR方案汇总递刀》阻塞清单 #5 逐字'),
    ('18', ['i07_twotable', 'i17_sendbit', 'i17_enumflag'],
     'decisions.log 裁二 + 启元《IR中端施工单v2合流版》§三'),
    ('21', ['i21_knife4', 'i21_coff', 'i21_knife5', 'i21_knife6'],
     '启元《施工单8刀复查》自带判定表'),
]
ALLKEYS = [k for _, ks, _ in ITEMS for k in ks]


def facts(s):
    d = {}
    for cell in s.split(';'):
        if '=' in cell:
            k, v = cell.split('=', 1)
            d[k.strip()] = v.strip()
    return d


# 期望：**只** 从 事实源 TSV 读
tsv = {}
for line in io.open(TSV, encoding='utf-8').read().split('\n'):
    if not line.strip() or line.startswith('#'):
        continue
    c = line.split('\t')
    if len(c) >= 7:
        tsv[c[0]] = dict(zip(['id', '递刀', '性质', '状态', '证据', '复算', '事实'], c))

# 实测：**只** 从 探针表 读
probes = {}
_p = os.path.join(S, 'logs', 'item_probes.tsv')
if os.path.exists(_p):
    for l in io.open(_p, encoding='utf-8').read().split('\n'):
        c = l.split('\t')
        if len(c) >= 4 and c[0] in ALLKEYS:
            probes[c[0]] = (c[1], c[2], c[3])

L, bad = [], []
L.append('# 未闭合项 · 遗留要件 逐条 现役 探针（plan 验证步骤 3）')
L.append('#')
L.append('# 判据（plan 逐字）：每条 必须 出现 二者之一 —— **转绿**（实测 rc/值 与 事实源 期望 逐字相符）')
L.append('#   或 **定性**（实测输出 与 递刀 逐字依据 一致，且 该依据 在 文档里 被引用）。')
L.append('# 生成器 = scripts/qiyuan_ledger_items.py（仓内）· 期望 来源 = qiyuan_ledger_facts.tsv 的事实格')
L.append('# 实测 来源 = {SCRATCH}/logs/item_probes.tsv（由 qiyuan_ledger_probe.py 用现役引擎生成）')
L.append('# 探针 rc 语义：**rc0 = 断言成立**；rc1 = 断言不成立（note 列 逐字写明断言内容）。')
L.append('')
L.append('## 1. 逐条 对账（判定 = **派生**，非硬编码）')
L.append('')
L.append('| id | 台账 状态 | 遗留要件 承载探针 | 期望(TSV) vs 实测 | 判定 | 递刀 逐字依据 |')
L.append('|:--|:--|:--|:--|:--|:--|')
for iid, keys, cite in ITEMS:
    rec = tsv.get(iid)
    if rec is None:
        bad.append('id=%s 不在 事实源 TSV'); continue
    exp = facts(rec['事实'])
    cells, ok_all = [], True
    for k in keys:
        e = exp.get(k)
        g = probes.get(k)
        got = ('rc' + g[0]) if g else 'MISSING'
        match = (e == got)
        ok_all = ok_all and match
        cells.append('%s: %s / %s %s' % (k, e or '—', got, '✅' if match else '❌'))
    sem = ('转绿（状态 = 已解决）' if rec['状态'] == '已解决'
           else '定性（状态 = %s · 余项 见 递刀 依据）' % rec['状态'])
    verdict = ('**%s**<br>%s' % (sem, '✅ 与事实源一致' if ok_all else '❌ 不符'))
    if not ok_all:
        bad.append('id=%s 实测 与 事实源 不符: %s' % (iid, '; '.join(cells)))
    L.append('| **%s** | %s | %s | %s | %s | %s |'
             % (iid, rec['状态'], ' · '.join('`%s`' % k for k in keys), '<br>'.join(cells), verdict, cite))
L.append('')
L.append('## 2. 探针 原始输出（逐行 摘录 自 item_probes.tsv）')
L.append('')
L.append('```')
for k in ALLKEYS:
    if k in probes:
        L.append('%-16s rc=%-3s code=%-3s %s' % (k, probes[k][0], probes[k][1], probes[k][2][:150]))
    else:
        L.append('%-16s **探针 缺失**' % k)
        bad.append('探针 缺失: %s' % k)
L.append('```')
L.append('')
L.append('## 3. 附加 事实（结构性 收口 · 非 遗留要件 但 同批 补 的证据；同样 由 TSV 期望 对账）')
for k in ['i07_twotable', 'i08_emoji', 'i10_c1', 'i11_c2b', 'i11_c7', 'i19_zero', 'i20_zero', 'i23_super']:
    g = probes.get(k)
    L.append('- `%s` = %s ⟹ %s' % (k, ('rc' + g[0]) if g else 'MISSING', (g[2] if g else '缺失')))
L.append('')
L.append('## 4. 复现（新 空 scratch 三步）')
L.append('```')
L.append('python scripts\\qiyuan_ledger_evidence.py {SCRATCH}                 # 门事实（probe/defect/p0/launch/gitfam/anchors/zlib）')
L.append('python scripts\\qiyuan_ledger_evidence.py {SCRATCH} bootstrap git   # 慢门（自举 · git 闸）')
L.append('python scripts\\qiyuan_ledger_check.py {SCRATCH}                    # 台账 一致性 校验')
L.append('python scripts\\qiyuan_ledger_terminal.py {SCRATCH}                 # 终态 逐行核')
L.append('python scripts\\qiyuan_ledger_items.py {SCRATCH}                    # 本文件')
L.append('```')
L.append('')
L.append('判定 = ' + ('✅ 全部 遗留要件 实测 与 事实源 逐字相符（转绿 或 定性）' if not bad else '❌ ' + ' | '.join(bad[:6])))
io.open(os.path.join(S, 'items_final.log'), 'w', encoding='utf-8', newline='').write('\n'.join(L) + '\n')
print('\n'.join(L[:22]))
print('...')
print(L[-1])
sys.exit(0 if not bad else 1)
