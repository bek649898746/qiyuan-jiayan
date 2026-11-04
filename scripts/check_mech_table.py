# -*- coding: utf-8 -*-
"""check_mech_table — `manual/机制特性实现表.md` 三处修正的可复算判据（ORD-20261102-86）。

判据：
  A 汇总：15 族 项数/✅/🔴/❌/定案 之和 == 合计行；总项 291、✅285、🔴1、❌5、定案31。
  B #204：状态 ✅；说明含 `两参=3 三参=6`；族 9 ✅ = 15。
  C #220：行必须恰好 4 个内容列；状态 ❌；含 `声明式` 6 处 / `依赖图` 4 处；不得再含「0 处」。
  D #120：状态 ❌；含 `普通 sum type` 与 `不提供 OCaml 式可扩展多态变体`；不得含「存疑」。
  E 真实编译器：#204 两参/三参重载输出 `两参=3 三参=6`；#120 `scripts/feat_adt_zh.c` 运行 PASS。
用法: python scripts/check_mech_table.py [--cc qcc_zh_v1.exe] [--table <另一份表>]
  `--table` 供修前回执用：同一支判据跑历史快照（`git show <rev>:manual/机制特性实现表.md`），
  修前必红、修后必绿，证明改的是表本身而不是判据的期望值。
"""
from qcc_tmp import tmp_dir, tmp_root
import argparse
import io
import os
import re
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLE = os.path.join(REPO, 'manual', '机制特性实现表.md')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    ap.add_argument('--table', default=TABLE)
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    table = a.table if os.path.isabs(a.table) else os.path.join(REPO, a.table)
    t = io.open(table, encoding='utf-8', errors='replace').read()
    lines = t.splitlines()
    bad = []

    # A 汇总
    fam = []
    total = None
    for ln in lines:
        m = re.match(r'^\|\s*\*?\*?(\d+)\*?\*?\s*\|([^|]*)\|\s*(\d+)\s*\|\s*\*?\*?(\d+)\*?\*?\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*([^|]*)\|', ln)
        if not m:
            continue
        n, name, items, ok, red, no, cases = int(m.group(1)), m.group(2).strip(), int(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6)), int(m.group(7))
        if n <= 15 and name and '合计' not in name:
            fam.append((n, name, items, ok, red, no, cases))
    for ln in lines:
        if '**合计**' in ln:
            nums = [int(x) for x in re.findall(r'\d+', ln.replace('15 族', ''))]
            if len(nums) >= 5:
                total = tuple(nums[:5])
            break
    sums = tuple(sum(x[i] for x in fam) for i in (2, 3, 4, 5, 6))
    if len(fam) != 15:
        bad.append('族数=%d != 15' % len(fam))
    if sums != (291, 285, 1, 5, 31):
        bad.append('族求和=%s != (291,285,1,5,31)' % (sums,))
    if total != (291, 285, 1, 5, 31):
        bad.append('合计行=%s != (291,285,1,5,31)' % (total,))
    f9 = [x for x in fam if x[0] == 9]
    if not f9 or f9[0][3] != 15:
        bad.append('#204 族9 ✅ != 15: %s' % (f9,))

    def row(pid):
        for ln in lines:
            if ln.strip().startswith('| %d ' % pid):
                return ln
        return ''

    r204 = row(204)
    if '| ✅ |' not in r204 or '两参=3 三参=6' not in r204:
        bad.append('#204 未转 ✅ / 缺实测输出: %s' % r204[:120])

    r220 = row(220)
    cells = [c.strip() for c in r220.strip().strip('|').split('|')]
    if len(cells) != 4:
        bad.append('#220 列数=%d != 4: %s' % (len(cells), r220[:120]))
    if '| ❌ |' not in r220 or '声明式' not in r220 or '6 处' not in r220 or '依赖图' not in r220 or '4 处' not in r220:
        bad.append('#220 说明/状态不符: %s' % r220[:160])
    if '0 处' in r220:
        bad.append('#220 仍含被证伪的「0 处」')

    r120 = row(120)
    if '| ❌ |' not in r120 or '普通 sum type' not in r120 or '不提供 OCaml 式可扩展多态变体' not in r120:
        bad.append('#120 终局/术语理由不符: %s' % r120[:160])
    if '存疑' in r120:
        bad.append('#120 仍含「存疑」')

    # E 真实编译器
    work = tmp_dir('qcc_mech')
    ov = os.path.join(work, 'ov204.c')
    io.open(ov, 'w', encoding='utf-8', newline='\n').write(
        '#include <stdio.h>\n'
        '函数 整 加(整 a, 整 b) { 返 a + b; }\n'
        '函数 整 加(整 a, 整 b, 整 c) { 返 a + b + c; }\n'
        '函数 整 主(空) { 格式化 "两参=%d 三参=%d\\n", 加(1, 2), 加(1, 2, 3); 返 0; }\n')
    ovx = os.path.join(work, 'ov204.exe')
    r = subprocess.run([cc, ov, '-o', ovx], cwd=REPO, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=180)
    if r.returncode != 0:
        bad.append('#204 示例编译失败 rc=%d %s' % (r.returncode, ((r.stdout or '') + (r.stderr or '')).strip()[:120]))
    else:
        q = subprocess.run([ovx], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
        if q.returncode != 0 or '两参=3 三参=6' not in (q.stdout or ''):
            bad.append('#204 示例输出不符 rc=%d out=%r' % (q.returncode, (q.stdout or '').strip()[:80]))

    adt = os.path.join(REPO, 'scripts', 'feat_adt_zh.c')
    adx = os.path.join(work, 'adt.exe')
    r = subprocess.run([cc, adt, '-o', adx], cwd=REPO, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=180)
    if r.returncode != 0:
        bad.append('#120 判别示例编译失败 rc=%d %s' % (r.returncode, ((r.stdout or '') + (r.stderr or '')).strip()[:120]))
    else:
        q = subprocess.run([adx], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
        if q.returncode != 0 or 'PASS' not in (q.stdout or ''):
            bad.append('#120 判别示例运行不符 rc=%d out=%r' % (q.returncode, (q.stdout or '').strip()[:80]))

    print('[表] %s' % table)
    print('[A] 15 族求和=%s 合计=%s' % (sums, total))
    print('[B] #204: %s' % ('PASS' if '两参=3 三参=6' in r204 and '| ✅ |' in r204 else 'FAIL'))
    print('[C] #220: 列=%d 状态=%s' % (len(cells), '❌' if '| ❌ |' in r220 else '?'))
    print('[D] #120: %s' % ('PASS' if '| ❌ |' in r120 and '不提供 OCaml 式可扩展多态变体' in r120 else 'FAIL'))
    print('[E] 真实编译器示例: %s' % ('PASS' if not any(x.startswith(('#204 示例', '#120 判别示例')) for x in bad) else 'FAIL'))
    for b in bad:
        print('[FAIL] ' + b)
    print('== 机制表判据: %s ==' % ('PASS' if not bad else 'FAIL (%d 处)' % len(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
