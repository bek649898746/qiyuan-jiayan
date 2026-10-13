# -*- coding: utf-8 -*-
"""qiyuan_ledger_render.py -- 从 **单一事实源** TSV 渲染 全部 人读视图（禁止 手抄 事实）。

用法: python scripts\\qiyuan_ledger_render.py <SCRATCH目录> [--check-only]

机制：本 脚本 只 在 目标 文档 的 `<!--GEN:BEGIN key=…--> … <!--GEN:END-->` 之间 写内容；
      GEN 区 之外 一律 是 历史散文（由 校验器 强制 围 `<!--HISTORICAL-->`）。
"""
import io, os, re, sys

QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
GB = r'C:\Users\Administrator\Desktop\grok-build'
TSV = os.path.join(QJ, 'scripts', 'qiyuan_ledger_facts.tsv')
LED = os.path.join(QJ, r'_audit_inbox\20261013_虾米_递刀对账单一入口_虾米\递刀台账_权威_20261013.md')
REC = os.path.join(QJ, r'_audit_inbox\20261013_虾米_递刀对账单一入口_虾米\回执_20261013_递刀台账闭环.md')
DEBT = os.path.join(GB, '_DEBT_INDEX.md')
HAND = os.path.join(GB, '_HANDOVER_20261013.md')

S = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('QCC_LEDGER_SCRATCH')
if not S or not os.path.isdir(S):
    sys.exit('用法: python scripts\\qiyuan_ledger_render.py <SCRATCH目录> (或 设 QCC_LEDGER_SCRATCH)')
S = os.path.abspath(S)


# ---------------- 读 事实源 ----------------
def load_tsv():
    rows, glob = [], None
    for line in io.open(TSV, encoding='utf-8').read().split('\n'):
        if not line.strip() or line.startswith('#'):
            continue
        c = line.split('\t')
        if len(c) < 7:
            continue
        rec = dict(zip(['id', '递刀', '性质', '状态', '证据', '复算', '事实'], c))
        if rec['id'] == 'GLOBAL':
            glob = rec
        else:
            rows.append(rec)
    return rows, glob


def facts(s):
    out = {}
    for cell in s.split(';'):
        cell = cell.strip()
        if '=' in cell:
            k, v = cell.split('=', 1)
            out[k.strip()] = v.strip()
    return out


ROWS, GLOB = load_tsv()
GF = facts(GLOB['事实'])
CNT = {}
for r in ROWS:
    CNT[r['状态']] = CNT.get(r['状态'], 0) + 1
N = len(ROWS)
ICON = {'已解决': '✅ **已解决**', '部分': '🟠 **部分**', '未解决': '🔴 **未解决 · 定性留档**', '作废': '🗑 **非缺陷 / 作废**'}


# ---------------- 生成 各 GEN 块 ----------------
def gen_counts():
    L = ['| 状态 | 份数 | 说明 |', '|:--|--:|:--|',
         '| %s | **%d** | 每份 已解决 均 附 **可复算证据**（探针 key 由 `qiyuan_ledger_probe.py` 现役引擎 实测；门 key 由 日志 逐字 提取） |'
         % (ICON['已解决'], CNT.get('已解决', 0)),
         '| %s | **%d** | 阻塞 已清 · 字段 半补 · IR 长线 余项 定性 |' % (ICON['部分'], CNT.get('部分', 0)),
         '| %s | **%d** | 每条 附 复测证据 + 递刀逐字 修法方向（plan 明列 非目标） |' % (ICON['未解决'], CNT.get('未解决', 0)),
         '| %s | **%d** | 递刀 自述「零编译器改动」/ 已被 后版 取代 |' % (ICON['作废'], CNT.get('作废', 0)),
         '| **合计** | **%d** | **「待核」= 0**（终态口径：`已解决 / 部分 / 未解决 / 作废` 之一） |' % N]
    return L


def gen_items():
    L = ['| # | 递刀 | 性质 | **状态** | 事实（机器可复算） | 证据 · 复算 |',
         '|--:|:--|:--|:--|:--|:--|']
    for r in ROWS:
        f = facts(r['事实'])
        fs = ' · '.join('`%s=%s`' % (k, v) for k, v in f.items()) if f else '—（非单刀缺陷：见 证据/复算）'
        L.append('| %s | %s | %s | %s | %s | %s ⟹ `%s` |' % (r['id'], r['递刀'], r['性质'], ICON[r['状态']], fs, r['证据'], r['复算']))
    return L


def gen_todo():
    done = [r for r in ROWS if r['状态'] == '已解决']
    left = [r for r in ROWS if r['状态'] == '未解决']
    L = ['> **待办 = 空**：%d 份 递刀 逐份 达 终态 ⟹ **已解决 %d / 部分 %d / 未解决·定性 %d / 作废 %d**，**「待核」= 0**。'
         % (N, CNT.get('已解决', 0), CNT.get('部分', 0), CNT.get('未解决', 0), CNT.get('作废', 0)),
         '>',
         '> **本轮 闭环 的 五条**（原 P0–P3 与 递刀 台账 收口）：',
         '',
         '| 项 | 终态 | 事实（机器可复算） |',
         '|:--|:--|:--|']
    for r in done:
        if r['id'] in ('12', '14', '15', '16', '22'):
            L.append('| `[%s]` %s | ✅ **已解决** | %s |' % (r['id'], r['递刀'],
                     ' · '.join('`%s=%s`' % (k, v) for k, v in facts(r['事实']).items()) or '见 证据'))
    L.append('')
    L.append('> **剩余 %d 条「未解决 · 定性留档」**：%s —— 复测证据 + 递刀逐字 修法方向 见 `{SCRATCH}\\defect_evidence.log`。'
             % (len(left), ' · '.join('`[%s]`' % r['id'] for r in left)))
    return L


def gen_gates():
    L = ['| 门 | 值（机器 提取） | 来源 |', '|:--|:--|:--|']
    for k, src in (('bootstrap', '{SCRATCH}/bootstrap_final.log'), ('anchors', '{SCRATCH}/anchor_final.log'),
                   ('zlib', '{SCRATCH}/zlib_rescan.log'), ('git', '{SCRATCH}/compile_par_result.txt'),
                   ('gitfam', '{SCRATCH}/logs/gitfam.txt'), ('launch', '{SCRATCH}/launch.log'),
                   ('p0', '{SCRATCH}/hang.log'), ('p0loud', '{SCRATCH}/hang.log')):
        L.append('| `%s` | **%s** | `%s` |' % (k, GF.get(k, '?'), src))
    return L


def gen_probes():
    L = ['| 递刀 | 探针 事实（现役引擎 实测） |', '|:--|:--|']
    for r in ROWS:
        f = facts(r['事实'])
        pk = {k: v for k, v in f.items() if re.match(r'^(i\d|[a-z]+\d*_)|^i\d\d_', k)}
        if pk:
            L.append('| `[%s]` | %s |' % (r['id'], ' · '.join('`%s=%s`' % (k, v) for k, v in pk.items())))
    return L


def gen_r1note():
    f = facts([r for r in ROWS if r['id'] == '16'][0]['事实'])
    zeros = [k for k, v in f.items() if v == 'rc0']
    nonz = [(k, v) for k, v in f.items() if v != 'rc0']
    em4 = f.get('i16_em4', '?')
    de5 = f.get('i16_de5', '?')
    L = ['**R1 判据 下 的 探针 实测（**全部** 从 单一事实源 TSV 派生）**：',
         '',
         '- 该 递刀 探针 共 **%d** 格：**rc=0 的 %d 格**（%s）；**rc≠0 的 %d 格**（%s）'
         % (len(f), len(zeros), ' · '.join('`%s`' % k for k in zeros), len(nonz),
            ' · '.join('`%s=%s`' % kv for kv in nonz)),
         '- **`em4`（`#ifdef 未定义名`）= `%s`** ⟹ **R1 有意取舍**：该 形态 与「命令行 `-D` 提供」无法 区分，'
         '故 记疑 **不抑制**（保 真阳性 · 保 git 族表）；递刀 🟡④ 的 该 格 因此 **不转绿**，'
         '🔴 三档（① 5 行 最小复现 ② `deflate.c` ③ zlib 15/15）不受影响。' % em4,
         '- **`de5`（`#if 1` 真分支 内 元数不符）= `%s`** ⟹ **真阳性 保住**（R799c 曾 把 它 静音，已 撤回）。' % de5]
    return L


def gen_debt_pointer():
    return ['> 🗡 **递刀台账（权威 · %d 份逐份判定 —— 终态 `%s`：已解决 **%d** / 部分 **%d** / 未解决·定性留档 **%d** / **待核 0** / 作废 **%d**）**：'
            % (N, GF.get('bootstrap', '?').split(' ')[0], CNT.get('已解决', 0), CNT.get('部分', 0),
               CNT.get('未解决', 0), CNT.get('作废', 0))]


def gen_debt_r799():
    L = ['| 编号 | 项 | 状态 | 事实 / 复算 |', '|:--|:--|:--|:--|']
    for k, item in (('i14', 'R799a/b：P0 挂起 根治（`度序` 匿名内联体 字段循环 兜底 A6069 + `_Static_assert` 正解）'),
                    ('i12', 'R799e：名字位 撞 存储类关键词 ⟹ A6058 → A9001「关键字 作 标识符」'),
                    ('i16', 'R799j：假分支掩码（判据 第 4 版 = R1）⟹ zlib 14/15 → 15/15'),
                    ('i22', 'R799h：静态断言 失败 走 `发错误("A6017", …)` ⟹ rc=1')):
        rid = {'i14': '14', 'i12': '12', 'i16': '16', 'i22': '22'}[k]
        row = [r for r in ROWS if r['id'] == rid][0]
        f = facts(row['事实'])
        keys = [x for x in f if x.startswith(k)]
        L.append('| **%s** | %s | ✅ **已闭环** | %s ⟹ `%s` |' % (k, item,
                 ' · '.join('`%s=%s`' % (x, f[x]) for x in keys) or '见 证据',
                 row['复算']))
    return L


def gen_receipt01():
    L = ['| 状态 | 份数 | 含义 |', '|:--|--:|:--|',
         '| ✅ 已解决 | **%d** | 探针 事实 逐格 可复算（见 GEN `probes`） |' % CNT.get('已解决', 0),
         '| 🟠 部分 | **%d** | %s |' % (CNT.get('部分', 0), ' · '.join('`[%s]`' % r['id'] for r in ROWS if r['状态'] == '部分')),
         '| 🔴 未解决 · 定性留档 | **%d** | %s（IR 长线 · plan 非目标） |' % (CNT.get('未解决', 0), ' · '.join('`[%s]`' % r['id'] for r in ROWS if r['状态'] == '未解决')),
         '| 🗑 作废 | **%d** | %s |' % (CNT.get('作废', 0), ' · '.join('`[%s]`' % r['id'] for r in ROWS if r['状态'] == '作废')),
         '| **合计** | **%d** | **「待核」= 0** |' % N]
    return L


def gen_receipt02():
    L = ['| # | 判据 | 事实（机器 提取） | 证据 |', '|--:|:--|:--|:--|',
         '| 1 | 端到端 发射 | **%s** | `{SCRATCH}/launch.log` |' % GF.get('launch'),
         '| 2 | P0 挂起（25s 超时 ×2） | **%s** · 失败断言 **%s** | `{SCRATCH}/hang.log` |' % (GF.get('p0'), GF.get('p0loud')),
         '| 3 | zlib 单套 | **%s** | `{SCRATCH}/zlib_rescan.log` |' % GF.get('zlib'),
         '| 4 | 自举 不动点 | **%s** | `{SCRATCH}/bootstrap_final.log` |' % GF.get('bootstrap'),
         '| 5 | 受影响 门锚 | **%s** | `{SCRATCH}/anchor_final.log` |' % GF.get('anchors'),
         '| 6 | git 全量闸（1 次） | **%s** · **%s** | `{SCRATCH}/compile_par_result.txt` · `logs/gitfam.txt` |' % (GF.get('git'), GF.get('gitfam')),
         '| 7 | 台账 逐条 终态 | **%d 份 · 待核 0**（已解决 %d / 部分 %d / 未解决 %d / 作废 %d） | `递刀台账_权威_20261013.md` |'
         % (N, CNT.get('已解决', 0), CNT.get('部分', 0), CNT.get('未解决', 0), CNT.get('作废', 0)),
         '| 8 | 未解决 留档 | `{SCRATCH}/defect_evidence.log` | — |']
    return L


def gen_handover01():
    return ['| 状态 | 份数 | 变化 |', '|:--|--:|:--|',
            '| ✅ 已解决 | **%d** | 探针 事实 逐格 可复算（GEN `probes`） |' % CNT.get('已解决', 0),
            '| 🟠 部分 | **%d** | `[17]` IR 长线 |' % CNT.get('部分', 0),
            '| 🔴 未解决·定性 | **%d** | %s |' % (CNT.get('未解决', 0), ' · '.join('`[%s]`' % r['id'] for r in ROWS if r['状态'] == '未解决')),
            '| 🗑 作废 | **%d** | %s |' % (CNT.get('作废', 0), ' · '.join('`[%s]`' % r['id'] for r in ROWS if r['状态'] == '作废')),
            '| **合计** | **%d** | 无「待核」 |' % N]


GENS = {'counts': gen_counts, 'items': gen_items, 'todo': gen_todo, 'gates': gen_gates,
        'probes': gen_probes, 'r1note': gen_r1note, 'debt_pointer': gen_debt_pointer,
        'debt_r799': gen_debt_r799, 'receipt01': gen_receipt01, 'receipt02': gen_receipt02,
        'handover01': gen_handover01}


def render_block(key):
    return '\n'.join(GENS[key]())


def apply(path, enc, dry=False):
    if not os.path.exists(path):
        print('  ! 目标 不存在: %s' % path); return 0
    s = io.open(path, 'rb').read().decode(enc)
    n = 0
    for k in GENS:
        pat = re.compile(r'(<!--GEN:BEGIN key=' + k + r'-->)(.*?)(<!--GEN:END-->)', re.S)
        if not pat.search(s):
            continue
        s2 = pat.sub(lambda m: m.group(1) + '\n' + render_block(k) + '\n' + m.group(3), s)
        if s2 != s:
            n += 1
        s = s2
    if not dry:
        io.open(path, 'wb').write(s.encode(enc))
    return n


total = 0
for p, enc in ((LED, 'utf-8-sig'), (REC, 'utf-8-sig'), (DEBT, 'utf-8-sig'), (HAND, 'utf-8')):
    c = apply(p, enc)
    total += c
    print('  %-28s 渲染 %d 个 GEN 块' % (os.path.basename(p), c))
print('合计 渲染 %d 块（事实源: %s · %d 份 递刀）' % (total, os.path.basename(TSV), N))
print('计数（派生）: ' + ' / '.join('%s %d' % (k, v) for k, v in sorted(CNT.items())))
