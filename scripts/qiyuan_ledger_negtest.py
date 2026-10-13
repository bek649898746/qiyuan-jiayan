# -*- coding: utf-8 -*-
"""qiyuan_ledger_negtest.py -- 台账判据 **负向测试**（证明三条判据 真的能失败，非恒真）。

用法: python scripts\\qiyuan_ledger_negtest.py <SCRATCH目录>
输出: {SCRATCH}/negtest.log

三条 负向（每条 都 对 **真文件** 篡改 → 断言 被抓 → 恢复 → 断言 复绿）:
  N1 判据③（GEN 块 逐字一致）      : 在 `_DEBT_INDEX.md` 的 GEN 块 内 插一行 ⟹ check 必须 报「不符 ≥1」
  N2 判据④（判定 从 实测 派生）    : 篡改 `logs/item_probes.tsv` 的 一个 rc ⟹ items 必须 报「不符」
  N3 判据②（复算列 必须 在仓）     : 把 事实源 的 一条 复算列 改指 不在仓 脚本 ⟹ terminal 必须 报「无 在仓」

背景（skeptic 复核 2026-10-14 打回）: 这三处 原先 都 是「恒真判据」或「硬编码判定」——
  ① render.py 无 `__main__` 保护 ⟹ check import 时 先 写盘 再 比对 ⟹ ③ 自我实现；
  ② items 的 判定 是 常量串，pass 条件 只看 key 存在 ⟹ 红探针 也 印「转绿」；
  ③ terminal 的「在仓」判据 只覆盖 未解决/部分行 ⟹ 已解决行 的 复算列 指向不在仓脚本 无人管。
"""
import io, os, re, shutil, subprocess, sys

QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
GB = r'C:\Users\Administrator\Desktop\grok-build'
S = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('QCC_LEDGER_SCRATCH', '')
if not S or not os.path.isdir(S):
    sys.exit('用法: python scripts\\qiyuan_ledger_negtest.py <SCRATCH目录>')
S = os.path.abspath(S)
CHK = os.path.join(QJ, 'scripts', 'qiyuan_ledger_check.py')
TERM = os.path.join(QJ, 'scripts', 'qiyuan_ledger_terminal.py')
ITEMS = os.path.join(QJ, 'scripts', 'qiyuan_ledger_items.py')
TSV = os.path.join(QJ, 'scripts', 'qiyuan_ledger_facts.tsv')
PROBES = os.path.join(S, 'logs', 'item_probes.tsv')
DEBT = os.path.join(GB, '_DEBT_INDEX.md')

L, fails = [], []


def run(script, *extra):
    r = subprocess.run([sys.executable, script, S] + list(extra), capture_output=True, cwd=QJ)
    return r.returncode, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')


def tamper(path, fn, label, script, expect_pat, header):
    rc0, _ = run(script)
    bk = path + '.negtest.bak'
    shutil.copy2(path, bk)
    orig = io.open(path, 'rb').read()
    t = orig.decode('utf-8-sig')
    t2 = fn(t)
    assert t2 != t, '[%s] 篡改 未生效' % label
    bom = b'\xef\xbb\xbf' if orig[:3] == b'\xef\xbb\xbf' else b''
    io.open(path, 'wb').write(bom + t2.encode('utf-8'))
    rc1, o1 = run(script)
    shutil.copy2(bk, path); os.remove(bk)
    rc2, _ = run(script)
    L.append('## %s' % header)
    L.append('- 基线: rc=%d' % rc0)
    L.append('- 篡改 `%s` ⟹ rc=%d' % (os.path.basename(path), rc1))
    for l in o1.split('\n'):
        if re.search(expect_pat, l):
            L.append('  > ' + l.strip()[:160])
    L.append('- 恢复后: rc=%d' % rc2)
    L.append('')
    ok = (rc0 == 0 and rc1 != 0 and rc2 == 0)
    if not ok:
        fails.append('%s: 基线rc=%d 篡改后rc=%d 恢复后rc=%d' % (label, rc0, rc1, rc2))
    return ok


L.append('# 台账判据 负向测试（三条 判据 必须 能失败）· 2026-10-14')
L.append('')

tamper(DEBT, lambda t: re.sub(r'(<!--GEN:BEGIN key=\w+-->\r?\n)', r'\g<1>ZZZZ-HAND-EDIT-PROBE-ZZZZ\n', t, count=1),
       'N1', CHK, r'GEN 块 一致性: 不符', 'N1 · 判据③ GEN 块（手改 GEN 块 必须 被 抓）')
tamper(PROBES, lambda t: re.sub(r'^(i13_ji\t)0\t', r'\g<1>1\t', t, count=0, flags=re.M),
       'N2', ITEMS, r'实测 . 事实源 不符|判定 = ', 'N2 · 判据④ 判定 派生（探针 rc 变红 必须 被 抓）')
tamper(TSV, lambda t: t.replace('qiyuan_ledger_probe.py {SCRATCH}（探针 i13_ji',
                                'no_such_script_zzz.py {SCRATCH}（探针 i13_ji', 1),
       'N3', TERM, r'无 \*\*在仓\*\*|未在仓最小复现=', 'N3 · 判据② 复算列 在仓（指向不在仓脚本 必须 被 抓）')
tamper(os.path.join(GB, '_HANDOVER_20261013.md'),
       lambda t: t.replace('\n', '\n> ZZZZ-围篱外-事实探针 DEAD1234 ZZZZ\n', 1),
       'N4', CHK, r'DEAD1234', 'N4 · 判据⑤ 围篱外事实（围篱/GEN 之外 写死 8 位哈希 必须 被 抓）')
# N5/N6/N7：skeptic 3 指出的 三个 漏抓面（本器 曾 全 漏）
tamper(os.path.join(GB, '_HANDOVER_20261013.md'),
       lambda t: t.replace('\n', '\n> ZZZZ-小写-8位哈希 dead1234 ZZZZ\n', 1),
       'N5', CHK, r'`dead1234`', 'N5 · 判据⑤ **小写** 8 位哈希（HEX 大小写敏感 会 漏抓）')
tamper(os.path.join(GB, '_HANDOVER_20261013.md'),
       lambda t: t.replace('\n', '\n> ZZZZ-7位-短哈希 4c886e8 ZZZZ\n', 1),
       'N6', CHK, r'`4c886e8`', 'N6 · 判据⑤ **7 位** git 短哈希（原 只认 8 位 ⟹ 漏抓）')
tamper(os.path.join(GB, '_HANDOVER_20261013.md'),
       lambda t: t.replace('\n', '\n> ZZZZ-gate计数值 total=999 ZZZZ\n', 1),
       'N7', CHK, r'`total=999`', 'N7 · 判据⑤ **gate 计数值**（与 GEN 重复 的 形态，原 无此模式）')

_n = sum(1 for _l in L if _l.startswith('- 恢复后'))
L.append('判定 = ' + ('✅ %d 条 判据 都 **能失败**（未篡改时 复绿 ⟹ 非恒真）' % _n if not fails
                    else '❌ ' + ' | '.join(fails)))
io.open(os.path.join(S, 'negtest.log'), 'w', encoding='utf-8', newline='').write('\n'.join(L) + '\n')
print('\n'.join(L))
sys.exit(0 if not fails else 1)
