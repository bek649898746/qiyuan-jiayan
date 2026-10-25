# -*- coding: utf-8 -*-
"""tautology_audit —— **「为绿而绿」恒真/自证断言审计**（plan 判据 3）。

规则（写死；命中数必须为 0）：
  A1 `@EXPECTED` 缺失（默认 exit:0 = 只断言「没崩」）；
  A2 只有 `exit:0`（无 out/err/in/warn/silent 内容断言），且同 stem 无 `*_neg*` 负控兄弟；
  A3 `out:` / `err:` / `warn:` / `in:` 值为空（断言恒真）；
  A4 锚文件不存在（登记了却跑不到）；
  A5 runner 在缺编译器时**静默 PASS**（应 rc=2 响亮拒绝）—— 静态断言 `sys.exit(2)` + `[SKIP]` 文案存在。
  A6 **台账判据脚本**在缺主体（编译器/被检文件/语料）时返回 0（假绿）——
     扫《_TASKLIST.json》里 fixed/blocked 行的 criterion_cmd，逐个脚本查 `[SKIP]` + `return 0` / `sys.exit(0)`。
     由来（2026-10-24 判据方打回）：原 A5 只扫 run_tests_zh.py，说「0 例」并不覆盖台账判据。

另报（不计入 A1–A5，单独列出供审计）：
  · `@KNOWN-GAP` 锚数（显式登记「应拒面缺失」）；
  · `@DEP` / `@MULTI`（依赖缺失即 SKIP 的豁免）；
  · CORE_FEAT 重复条目数。

用法: python scripts/tautology_audit.py [--log FILE]
退出码: 0 = A1–A5 全 0；1 = 有命中
"""
import argparse
import io
import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNNER = os.path.join(ROOT, 'scripts', 'run_tests_zh.py')
ASSERT_KINDS = ('out:', 'err:', 'in:', 'warn:', 'silent:')


def core_feat():
    out, depth = [], 0
    for line in io.open(RUNNER, encoding='utf-8', errors='replace').read().split('\n'):
        t = line.strip()
        if 'CORE_FEAT' in t and '[' in t:
            depth += t.count('[') - t.count(']')
            continue
        if depth > 0:
            if t.startswith('#') or t.startswith('//'):
                continue
            out += re.findall(r"'(scripts/[^']+\.c|tests/[^']+\.c|behavior/[^']+\.c|qcc/[^']+\.c)'", line)
            depth += t.count('[') - t.count(']')
    return out


def abspath(rel):
    for c in (rel, 'tests/' + rel):
        p = os.path.join(ROOT, c.replace('/', os.sep))
        if os.path.exists(p):
            return p
    return None


def stem(name):
    b = os.path.basename(name)[:-2]
    for suf in ('_ok', '_neg2', '_neg3', '_neg', '_ok2'):
        if b.endswith(suf):
            return b[:-len(suf)]
    return b


def scan_ledger_criteria(ledger):
    """返回 (hits, 已扫脚本数)。hits = [(script, 位置/原因)]。

    判据：脚本里出现 `[SKIP]` 文案，且 **同一 5 行窗口 / 同行** 内有
    `return 0` 或 `sys.exit(0)` ⇹ 「缺主体即 PASS」。注释行不算。
    """
    import json
    p = os.path.join(ledger, '_TASKLIST.json')
    if not os.path.exists(p):
        return [], 0
    rows = json.load(io.open(p, encoding='utf-8'))['rows']
    names = set()
    for r in rows:
        if r.get('status') not in ('fixed', 'blocked'):
            continue
        for m in re.finditer(r'scripts/([A-Za-z0-9_]+\.py)', r.get('criterion_cmd') or ''):
            names.add(m.group(1))
    hits = []
    for name in sorted(names):
        sp = os.path.join(ROOT, 'scripts', name)
        if not os.path.exists(sp):
            hits.append((name, '脚本不存在'))
            continue
        L = io.open(sp, encoding='utf-8', errors='replace').read().split('\n')
        for i, l in enumerate(L):
            t = l.strip()
            if '[SKIP]' not in t or t.startswith('#'):
                continue
            window = '\n'.join(L[i:i + 5])
            if re.search(r'return\s+0|sys\.exit\(0\)', window):
                hits.append((name, 'L%d: %s' % (i + 1, t[:70])))
    return hits, len(names)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--log', default='')
    ap.add_argument('--ledger', default=os.environ.get('QCC_LEDGER', os.path.join(os.path.dirname(ROOT), 'grok-build')),
                    help='台账仓路径（默认 = 编译器仓的姐妹目录 grok-build）')
    a = ap.parse_args()
    entries = core_feat()
    uniq = list(dict.fromkeys(entries))
    dup = len(entries) - len(uniq)
    disk = [os.path.basename(p) for p in
            __import__('glob').glob(os.path.join(ROOT, 'scripts', 'feat_*.c'))]
    A = {'A1': [], 'A2': [], 'A3': [], 'A4': []}
    known_gap, exempt = [], []
    for rel in uniq:
        p = abspath(rel)
        if p is None:
            A['A4'].append(rel); continue
        txt = io.open(p, encoding='utf-8', errors='replace').read()
        lines = [m.group(1).strip() for m in re.finditer(r'//\s*@EXPECTED\s+(.+)', txt)]
        joined = ' '.join(lines)
        if '@KNOWN-GAP' in txt:
            known_gap.append(rel)
        if '@DEP' in txt or '@MULTI' in txt:
            exempt.append(rel)
        if not lines:
            A['A1'].append(rel); continue
        for k in ASSERT_KINDS:
            if k in joined and joined.split(k, 1)[1].strip() == '':
                A['A3'].append(rel); break
        content = any(k in joined and joined.split(k, 1)[1].strip() for k in ASSERT_KINDS)
        neg = ('compile_fail' in joined) or bool(re.search(r'exit:\s*(?!0\b)\S+', joined))
        if content or neg:
            continue
        sib = [q for q in disk if q != os.path.basename(rel) and stem(q) == stem(rel) and '_neg' in q]
        if not sib:
            A['A2'].append(rel)
    # A5：runner 缺编译器必须 rc=2（不是静默 PASS）
    rs = io.open(RUNNER, encoding='utf-8', errors='replace').read()
    a5 = not ('拒绝退化到英文链' in rs and re.search(r'^\s*sys\.exit\(2\)', rs, re.M))
    # A6：台账判据脚本 缺主体 不得当绿（扫 criterion_cmd 指向的每个脚本）
    a6_hits, a6_checked = scan_ledger_criteria(a.ledger)
    a6 = len(a6_hits) + (1 if a6_checked == 0 else 0)
    lines = ['== tautology_audit ==', 'CORE_FEAT 条目=%d 去重=%d 重复=%d' % (len(entries), len(uniq), dup)]
    for k in ('A1', 'A2', 'A3', 'A4'):
        lines.append('%s: %d %s' % (k, len(A[k]), A[k][:8]))
    lines.append('A5 缺编译器静默 PASS: %s' % ('YES（缺陷）' if a5 else 'NO（rc=2 响亮拒绝）'))
    lines.append('A6 台账判据缺主体即 PASS: %d 命中（扫了 %d 个判据脚本）%s'
                 % (len(a6_hits), a6_checked, '' if a6_checked else '（台账未找到: %s）' % a.ledger))
    for nm, why in a6_hits[:10]:
        lines.append('  A6 %s  %s' % (nm, why))
    lines.append('@KNOWN-GAP: %d %s' % (len(known_gap), known_gap[:8]))
    lines.append('@DEP/@MULTI 豁免: %d' % len(exempt))
    total = sum(len(v) for v in A.values()) + (1 if a5 else 0) + a6
    text = '\n'.join(lines)
    print(text)
    if a.log:
        io.open(a.log, 'w', encoding='utf-8', newline='\n').write(text + '\n')
    print('=== 恒真审计: %s（命中 %d）===' % ('PASS' if total == 0 else 'FAIL', total))
    return 0 if total == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
