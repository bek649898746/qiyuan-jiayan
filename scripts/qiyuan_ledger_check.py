# -*- coding: utf-8 -*-
"""qiyuan_ledger_check.py -- 台账 **真校验器**（判据 1 的 机器 证据）。

用法: python scripts\\qiyuan_ledger_check.py <SCRATCH目录> [--no-live]

五项：
 ① **计数 派生**：从 TSV 的 状态 列 重新 数（不 读 任何 文档）。
 ② **逐格 核 事实**：TSV 每个 `key=value` 格 —— 探针 key 对 `{SCRATCH}/logs/item_probes.tsv`
    （由 `qiyuan_ledger_probe.py` 用 **现役引擎** 实测）；门 key 对 **live 复跑**（zlib/launch/p0）
    或 对 已捕获 日志（bootstrap/anchors/git，长任务）。**任一 不符 ⟹ FAIL**。
 ③ **GEN 一致性**：每份 文档 的 每个 `<!--GEN:BEGIN key=…-->` 块，必须 与 渲染器 当前 输出 **逐字 相同**
    （⟹ 手改 生成块 会 被 抓）；同 key 的 多 处 必须 相互 一致。
 ④ **历史 隔离**：`<!--HISTORICAL-->` 围篱 内 的 行，若 提及 状态/取值 断言，必须 带 历史 标记。
 ⑤ **事实 单一来源**：GEN 块 与 HISTORICAL 围篱 **之外** 的 正文，不得 出现 探针 id / 短名
    （⟹ 事实 只能 来自 TSV⟹渲染）。
退出码 0 = 全过；1 = 有 不一致。
"""
import io, os, re, subprocess, sys, importlib.util, hashlib


def sha_file(p):
    return hashlib.sha256(io.open(p, 'rb').read()).hexdigest()

QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
GB = r'C:\Users\Administrator\Desktop\grok-build'
SCRIPTS = os.path.join(QJ, 'scripts')
TSV = os.path.join(SCRIPTS, 'qiyuan_ledger_facts.tsv')
LED = os.path.join(QJ, r'_audit_inbox\20261013_虾米_递刀对账单一入口_虾米\递刀台账_权威_20261013.md')
REC = os.path.join(QJ, r'_audit_inbox\20261013_虾米_递刀对账单一入口_虾米\回执_20261013_递刀台账闭环.md')
DEBT = os.path.join(GB, '_DEBT_INDEX.md')
HAND = os.path.join(GB, '_HANDOVER_20261013.md')
DOCS = ((LED, 'utf-8-sig'), (REC, 'utf-8-sig'), (DEBT, 'utf-8-sig'), (HAND, 'utf-8'))

S = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('QCC_LEDGER_SCRATCH')
if not S or not os.path.isdir(S):
    sys.exit('用法: python scripts\\qiyuan_ledger_check.py <SCRATCH目录> (或 设 QCC_LEDGER_SCRATCH)')
S = os.path.abspath(S)
NOLIVE = '--no-live' in sys.argv
EXE = os.path.join(QJ, 'qcc_zh_v2.exe')
PROBE_TSV = os.path.join(S, 'logs', 'item_probes.tsv')

fails, rep = [], []


def out(s):
    print(s); rep.append(s)


def rd(p):
    if not os.path.exists(p):
        return None
    raw = io.open(p, 'rb').read()
    for enc in ('utf-8-sig', 'utf-16', 'gbk'):
        try:
            return raw.decode(enc)
        except Exception:
            pass
    return raw.decode('latin-1')


# ---------- ① 计数 ----------
rows = []
glob = None
for line in io.open(TSV, encoding='utf-8').read().split('\n'):
    if not line.strip() or line.startswith('#'):
        continue
    c = line.split('\t')
    if len(c) < 7:
        continue
    rec = dict(zip(['id', '递刀', '性质', '状态', '证据', '复算', '事实'], c))
    (glob := rec) if rec['id'] == 'GLOBAL' else rows.append(rec)
CNT = {}
for r in rows:
    CNT[r['状态']] = CNT.get(r['状态'], 0) + 1
out('① 计数（从 TSV 派生）: 合计 %d · ' % len(rows) + ' · '.join('%s %d' % (k, CNT[k]) for k in sorted(CNT)))
if sum(CNT.values()) != len(rows) or '待核' in CNT:
    fails.append('① 计数 异常')


def facts(s):
    d = {}
    for cell in s.split(';'):
        if '=' in cell:
            k, v = cell.split('=', 1)
            d[k.strip()] = v.strip()
    return d


# ---------- ② 逐格 核 事实 ----------
probes = {}
t = rd(PROBE_TSV)
if t is None:
    fails.append('② 探针 事实表 缺失（先跑 qiyuan_ledger_probe.py）')
else:
    for l in t.split('\n'):
        if not l.strip() or l.startswith('#') or l.startswith('key\t'):
            continue
        c = l.split('\t')
        if len(c) >= 3:
            probes[c[0]] = (c[1], c[2])


def probe_val(k):
    if k not in probes:
        return None
    rc, code = probes[k]
    return 'rc%s%s' % (rc, (':' + code) if code not in ('-', '') else '')


def live_or_log_zlib():
    if not NOLIVE:
        p = os.path.join(S, 'logs', 'live_zlib.log')
        r = subprocess.run([sys.executable, os.path.join(SCRIPTS, 'qiyuan_ledger_zlib_scan.py'), '--cc', EXE,
                            '--parallel', '4', 'zlib'], capture_output=True, cwd=QJ, timeout=600)
        txt = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
        io.open(p, 'w', encoding='utf-8', newline='').write(txt)
    else:
        txt = rd(os.path.join(S, 'zlib_rescan.log')) or ''
    m = re.search(r'zlib\s+(\d+)/(\d+)\s*通过', txt)
    return ('%s/%s' % (m.group(1), m.group(2))) if m else None


def gate_val(k):
    if k == 'zlib':
        return live_or_log_zlib()
    if k == 'bootstrap':
        t = rd(os.path.join(S, 'bootstrap_final.log')) or ''
        m = re.search(r'不动点\s+([0-9A-Fa-f]{8})\s+达成', t)
        return ('%s · PASS' % m.group(1).upper()) if (m and '[通过 PASS]' in t) else None
    if k == 'anchors':
        t = rd(os.path.join(S, 'anchor_final.log')) or ''
        m = re.search(r'PASS=(\d+)\s+SKIP=(\d+)\s+FAIL=(\d+)', t)
        return ('PASS=%s FAIL=%s' % (m.group(1), m.group(3))) if m else None
    if k == 'git':
        t = rd(os.path.join(S, 'compile_par_result.txt')) or ''
        m = re.search(r'total=(\d+)\s+OK=(\d+)\s+FAIL=(\d+)\s+CRASH=(\d+)', t)
        return ('total=%s OK=%s FAIL=%s CRASH=%s' % m.groups()) if m else None
    if k == 'gitfam':
        t = rd(os.path.join(S, 'logs', 'gitfam.txt')) or ''
        return '无上升' if '★ 上升的族: 无' in t else None
    if k == 'launch':
        if not NOLIVE:
            # R802 (判据 4): live 生成器 改为 **仓内** 统一证据重建器（原先 指向 {SCRATCH}/tools/launch_test.py，
            # scratch 一清空 就 失效 ⟹ 检查器 无法 在 干净环境 复跑）。
            p = os.path.join(S, 'logs', 'live_launch.log')
            r = subprocess.run([sys.executable, os.path.join(SCRIPTS, 'qiyuan_ledger_evidence.py'), S, 'launch'],
                               capture_output=True, cwd=QJ, timeout=300)
            io.open(p, 'w', encoding='utf-8', newline='').write((r.stdout or b'').decode('utf-8', 'replace'))
        t = rd(os.path.join(S, 'launch.log')) or ''
        m = re.search(r"stdout='([^']+)'\s+stderr=", t)
        ok2 = len(re.findall(r"rc=0\s+stdout='LAUNCH 870 55'", t)) >= 2
        return ('%s · rc=0 ×2' % m.group(1)) if (m and ok2) else None
    if k == 'p0':
        t = rd(os.path.join(S, 'hang.log')) or ''
        rcs = re.findall(r'p0_min\s+第 (\d) 次: rc=(-?\d+)', t)
        return 'rc=0 ×2 一致' if len(rcs) == 2 and all(x[1] == '0' for x in rcs) else None
    if k == 'p0loud':
        t = rd(os.path.join(S, 'hang.log')) or ''
        m = re.search(r'p0_false\s+第 1 次: rc=(-?\d+)\s+\[ERR\] (A\d{4})', t)
        return ('rc=%s · %s' % (m.group(1), m.group(2))) if m else None
    return None


GATESET = ('zlib', 'bootstrap', 'anchors', 'git', 'gitfam', 'launch', 'p0', 'p0loud')
bad = 0
for r in rows + [glob]:
    if r is None or r['事实'].strip() in ('-', ''):
        continue
    for k, v in facts(r['事实']).items():
        got = gate_val(k) if k in GATESET else probe_val(k)
        if got != v:
            bad += 1
            out('   ✗ [%s] %s: 台账 写 `%s` · 实测/提取 `%s`' % (r['id'], k, v, got))
# 证据 路径 存在性
missp = []
for r in rows + [glob]:
    if r is None:
        continue
    for p in r['证据'].split(';'):
        pp = p.strip().replace('{SCRATCH}', S)
        if pp and not os.path.exists(pp):
            missp.append((r['id'], pp))
out('② 事实 逐格: 不符 %d · 证据 路径 缺失 %d' % (bad, len(missp)))
for x in missp[:6]:
    out('   ✗ 缺失 [%s] %s' % x)
if bad or missp:
    fails.append('② 事实 不符/证据 缺失')

# ---------- ③ GEN 一致性（与 渲染器 现输出 逐字 比） ----------
# ⚠️ gap3 修 (2026-10-14): 先 **记 4 份文档 的 sha256**，import render 之后 必须 逐字节不变。
#   否则 = check 自己 把 文档 重写成 与 GENS 一致 ⟹ 判据③ 自我实现（手改 GEN 块 被抓不到），
#   且 校验器 每次运行 都 改写 工作区（判定所依据的产物 被 自身 修改）—— 两者 都 是 真缺陷。
_h_before = {p: (sha_file(p) if os.path.exists(p) else None) for p, _e in DOCS}
os.environ['QCC_LEDGER_SCRATCH'] = S
spec = importlib.util.spec_from_file_location('rnd', os.path.join(SCRIPTS, 'qiyuan_ledger_render.py'))
rnd = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rnd)      # render.py 顶层 有 `if __name__ == '__main__'` 保护 ⟹ 此处 **不写盘**
_h_after = {p: (sha_file(p) if os.path.exists(p) else None) for p, _e in DOCS}
_touched = [os.path.basename(p) for p in _h_before if _h_before[p] != _h_after[p]]
out('③b 校验器 只读: import render 后 文档 被改写 的 份数 = %d %s' % (len(_touched), _touched))
if _touched:
    fails.append('③b 校验器 改写了 工作区（import render 触发 写盘 ⟹ ③ 自我实现）')
genbad = 0
for p, enc in DOCS:
    s = rd(p)
    if s is None:
        out('   ✗ 文档 缺失: %s' % p); genbad += 1; continue
    for m in re.finditer(r'<!--GEN:BEGIN key=(\w+)-->(.*?)<!--GEN:END-->', s, re.S):
        k, body = m.group(1), m.group(2).strip('\r\n')
        want = '\n'.join(rnd.GENS[k]()).strip('\r\n')
        if body != want:
            genbad += 1
            out('   ✗ GEN 块 `%s`（%s）与 渲染器 输出 不符' % (k, os.path.basename(p)))
out('③ GEN 块 一致性: 不符 %d' % genbad)
if genbad:
    fails.append('③ GEN 块 不符')

# ---------- ④ 历史 隔离（围篱 必须 带 历史 标签 · 围篱 内 不得 有 GEN 块） ----------
LABEL = ('历史', '时点', '已 被', '附录', '按轮次', '遗留', '撤回', '上版', '起点')
hisbad = []
for p, enc in DOCS:
    s = rd(p)
    if s is None:
        continue
    for m in re.finditer(r'<!--HISTORICAL-->(.*?)<!--/HISTORICAL-->', s, re.S):
        body = m.group(1)
        head = [l for l in body.split('\n') if l.strip()][:5]
        if not any(any(k in l for k in LABEL) for l in head):
            hisbad.append((os.path.basename(p), '围篱 前 5 行 无 历史 标签'))
        if '<!--GEN:BEGIN' in body:
            hisbad.append((os.path.basename(p), '围篱 内 出现 GEN 块'))
out('④ 历史 隔离: 围篱 无标签/内嵌 GEN = %d' % len(hisbad))
for x in hisbad[:5]:
    out('   ✗ %s: %s' % x)
if hisbad:
    fails.append('④ 历史 围篱 不合规')

# ---------- ⑤ 事实 单一来源 ----------
SHORT = r'(?<![A-Za-z0-9_])(em1|em2|em4|em7|em8|de1|de2|de4|de5|i\d\d_[a-z0-9_]+|p0_hang|p0_loud)(?![A-Za-z0-9_])'
srcbad = []
for p, enc in DOCS:
    s = rd(p)
    if s is None:
        continue
    masked = re.sub(r'<!--HISTORICAL-->.*?<!--/HISTORICAL-->', '', s, flags=re.S)
    masked = re.sub(r'<!--GEN:BEGIN key=\w+-->.*?<!--GEN:END-->', '', masked, flags=re.S)
    for ln in masked.split('\n'):
        if re.search(SHORT, ln):
            srcbad.append((os.path.basename(p), ln.strip()[:80]))
out('⑤ 事实 单一来源: GEN/历史 之外 出现 探针 标识 %d' % len(srcbad))
for x in srcbad[:6]:
    out('   ✗ %s: %s' % x)
if srcbad:
    fails.append('⑤ GEN/历史 之外 有 探针 事实')

out('')
out('判定 = ' + ('✅ 台账 一致性 全过（事实 单一来源 · 逐格 可复算 · 历史 隔离）' if not fails else '❌ ' + str(fails)))
io.open(os.path.join(S, 'logs', 'ledger_check.txt'), 'w', encoding='utf-8', newline='').write('\n'.join(rep) + '\n')
sys.exit(0 if not fails else 1)
