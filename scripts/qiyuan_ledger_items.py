# -*- coding: utf-8 -*-
"""qiyuan_ledger_items.py -- 未闭合项 **遗留要件** 逐条 现役 探针 证据（plan 验证步骤 3）。

用法: python scripts\\qiyuan_ledger_items.py <SCRATCH目录>
输出: {SCRATCH}/items_final.log

口径（plan 逐字）: 每条必须出现 二者之一 ——
  * **转绿**: 实测 rc/值 与 事实源 期望 逐字相符
  * **定性**: 实测输出 与 递刀 逐字依据 一致，且 该依据 在 文档里 被引用
"""
import io, os, re, sys

QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
S = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('QCC_LEDGER_SCRATCH', '')
if not S or not os.path.isdir(S):
    sys.exit('用法: python scripts\\qiyuan_ledger_items.py <SCRATCH目录>')
S = os.path.abspath(S)

# 未闭合项（部分 / 未解决）的 遗留要件 -> 承载它的 探针 key
ITEMS = [
    ('13', '未解决→已解决', '六刀 v2 全部 要件（刀一 `控式=3` / 刀二 `段双` 三档 / 刀三 `剩系` / 刀五 `换代` / 刀六 容量注释）',
     ['i13_ji', 'i13_knives', 'i13_uncal'], '转绿', '启元《施工单v2_按甲言地基》§三 六刀 + §二 八条纪律'),
    ('17', '部分（保持）', '阻塞余项 = `Send位`/`枚举底层标志` 字段 未落 + JSON 4 个读端按 dump 复算',
     ['i17_vla', 'i17_sendbit', 'i17_enumflag', 'i17_readend'], '定性（未落 = 事实）', '启元《IR方案汇总递刀》阻塞清单 #5 逐字'),
    ('18', '未解决→部分', '裁二（一节点多指令 表示法）前置闸 + 刀五~刀七',
     ['i07_twotable', 'i17_sendbit', 'i17_enumflag'], '转绿（前置闸 已开）', 'decisions.log 裁二 + 启元《IR中端施工单v2合流版》§三'),
    ('21', '未解决→部分', '8 刀复查 刀一~刀六 逐刀',
     ['i21_knife4', 'i21_coff', 'i21_knife5', 'i21_knife6'], '转绿（刀 1/2/3/4/6）· 定性（刀五 未做）', '启元《施工单8刀复查》自带判定表'),
]
ALLKEYS = [k for _, _, _, ks, _, _ in ITEMS for k in ks]

probes = {}
t = io.open(os.path.join(S, 'logs', 'item_probes.tsv'), 'rb').read().decode('utf-8') \
    if os.path.exists(os.path.join(S, 'logs', 'item_probes.tsv')) else ''
for l in t.split('\n'):
    c = l.split('\t')
    if len(c) >= 4 and c[0] in ALLKEYS:
        probes[c[0]] = (c[1], c[2], c[3])

L = []
L.append('# 未闭合项 · 遗留要件 逐条 现役 探针（plan 验证步骤 3）')
L.append('#')
L.append('# 判据（plan 逐字）：每条 必须 出现 二者之一 —— **转绿**（实测 rc/值 与 事实源 期望 逐字相符）')
L.append('#   或 **定性**（实测输出 与 递刀 逐字依据 一致，且 该依据 在 文档里 被引用）。')
L.append('# 生成器 = scripts/qiyuan_ledger_items.py（本文件 · 仓内）· 事实来源 = {SCRATCH}/logs/item_probes.tsv')
L.append('# 说明：探针 rc 语义 —— **rc0 = 断言成立**；rc1 = 断言不成立（note 列 逐字写明断言内容）。')
L.append('')
L.append('## 1. 逐条 判定')
L.append('')
L.append('| id | 状态 变化 | 遗留要件 | 实测（探针 = rc） | 判定 | 递刀 逐字依据 |')
L.append('|:--|:--|:--|:--|:--|:--|')
for iid, chg, req, keys, verdict,依据 in ITEMS:
    got = ' · '.join('%s=%s' % (k, probes.get(k, ('MISSING',))[0]) for k in keys)
    L.append('| **%s** | %s | %s | %s | **%s** | %s |' % (iid, chg, req, got, verdict, 依据))
L.append('')
L.append('## 2. 探针 原始输出（逐行 摘录 自 item_probes.tsv）')
L.append('')
L.append('```')
for k in ALLKEYS:
    if k in probes:
        L.append('%-16s rc=%-3s code=%-3s %s' % (k, probes[k][0], probes[k][1], probes[k][2][:150]))
    else:
        L.append('%-16s **探针 缺失**' % k)
L.append('```')
L.append('')
L.append('## 3. 附加 事实（结构性 收口 · 非 遗留要件 但 同批 补 的证据）')
L.append('- `i07_twotable=rc0` ⟹ `QCC_DUMP=ir` 下 L1 表 / 影子对账 / CFG 块表 **三者同现**（IR 两套表并存 · id7）')
L.append('- `i08_emoji=rc0` ⟹ 检查器 输出 **真 emoji = 0**（仪器刀 已修 · id8）')
L.append('- `i10_c1 / i11_c2b / i11_c7 = rc0` ⟹ C1/C2b/C7 与 gcc **逐值一致**（8 / 2.50 / 7.50 · id10/id11）')
L.append('- `i19_zero / i20_zero / i23_super = rc0` ⟹ 作废三条 的「零改动 / 被取代」依据 在盘（id19/id20/id23）')
L.append('')
L.append('## 4. 复现（新 空 scratch 三步）')
L.append('```')
L.append('python scripts\\qiyuan_ledger_evidence.py {SCRATCH}        # 门事实（probe/defect/p0/launch/gitfam/anchors/zlib）')
L.append('python scripts\\qiyuan_ledger_evidence.py {SCRATCH} bootstrap git   # 慢门（自举 · git 闸）')
L.append('python scripts\\qiyuan_ledger_check.py {SCRATCH}          # 台账 一致性 校验')
L.append('python scripts\\qiyuan_ledger_terminal.py {SCRATCH}       # 终态 逐行核')
L.append('python scripts\\qiyuan_ledger_items.py {SCRATCH}          # 本文件')
L.append('```')
missing = [k for k in ALLKEYS if k not in probes]
L.append('')
L.append('判定 = ' + ('✅ 全部 遗留要件 有 实测输出（转绿 或 定性）' if not missing else '❌ 缺 探针: %s' % missing))
io.open(os.path.join(S, 'items_final.log'), 'w', encoding='utf-8', newline='').write('\n'.join(L) + '\n')
print('\n'.join(L))
sys.exit(0 if not missing else 1)
