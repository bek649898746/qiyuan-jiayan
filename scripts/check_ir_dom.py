#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ir_dom —— D2/D3 支配森林判据（SSA 地基）。

修前（D2）只验证了 `kw3_a/b` 两个**单函数**探针，且假定「根 = 块 0」。
D3 起 CFG 有函数边界 ⟹ 支配树是**多根森林**（每函数一个根），根不再是块 0。

断言（读真实 `QCC_DUMP=ir` dump）：
  1. `IR 支配树` 段存在，块数 == 上退，**可达块** `未定=0`；
  2. 每块 idom ∈ [0,块数)；idom 与子块 **同函数**（跨函数 = 红）；
  3. 函数首块 必须是根（idom==自身）；根 `深==0`；非根 `深 == 深[idom]+1`；
  4. idom 链在有限步内到**同函数的根**；
  5. 负例：idom 越界 / 跨函数 / 成环，同一 verifier 必须判红；
  6. 零产物影响：加/不加 `QCC_DUMP=ir`，产物 SHA256 相同。

用法: python scripts/check_ir_dom.py [--cc qcc_zh_seed.exe]
退出码: 0 = PASS / 1 = RED
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROBE = os.path.join(REPO, 'scripts', 'probe')
PAT = re.compile(r'支\[(\d+)\] idom=(-?\d+) 深=(-?\d+) 函号=(\d+)')
HDR = re.compile(r'IR 支配树 \(迭代 idom · 块数=(\d+) · 未定=(\d+)\)')
RCH = re.compile(r'IR 支配树 可达性 \(可达=(\d+) · 不可达=(\d+)\)')
FUN = re.compile(r'  函数\[(\d+)\] 首节点=(\d+) 块=(\d+)')


def dec(b):
    for enc in ('utf-8', 'gbk'):
        try:
            return b.decode(enc)
        except UnicodeDecodeError:
            pass
    return b.decode('utf-8', 'replace')


def run(args, cwd, env=None):
    e = dict(os.environ)
    if env:
        e.update(env)
    r = subprocess.run(args, cwd=cwd, capture_output=True, env=e)
    return r.returncode, dec((r.stdout or b'') + (r.stderr or b''))


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for c in iter(lambda: f.read(1 << 20), b''):
            h.update(c)
    return h.hexdigest()


def verify(text):
    bad = []
    m = HDR.search(text)
    if not m:
        return ['缺 支配树 段']
    n, und = int(m.group(1)), int(m.group(2))
    if n <= 0:
        bad.append('块数=0')
    if und != 0:
        bad.append('未定=%d' % und)
    rows = [(int(a), int(b), int(c), int(d)) for a, b, c, d in PAT.findall(text)]
    if len(rows) != n:
        bad.append('行 %d != 块数 %d' % (len(rows), n))
    dom = {a: b for a, b, c, d in rows}
    dep = {a: c for a, b, c, d in rows}
    fn = {a: d for a, b, c, d in rows}
    heads = {int(bid): int(fid) for fid, _st, bid in FUN.findall(text)}
    for a, b, c, d in rows:
        if b < 0:
            # D3: 不可达块（终结符后的死尾）idom=-1 / 深=-1 —— 显式允许，不冒充「未定」
            if c != -1:
                bad.append('支[%d] 不可达 深=%d != -1' % (a, c))
            continue
        if not (0 <= b < n):
            bad.append('支[%d] idom=%d 越界' % (a, b))
            continue
        if b != a and fn.get(b) != d:
            bad.append('支[%d] idom=%d 跨函数 (%d→%d)' % (a, b, d, fn.get(b)))
            continue
        if b == a:
            if c != 0:
                bad.append('支[%d] 根 深=%d != 0' % (a, c))
            if a in heads and heads[a] != d:
                bad.append('支[%d] 函数首 函号=%d != %d' % (a, d, heads[a]))
            continue
        if dep.get(b) is None or c != dep[b] + 1:
            bad.append('支[%d] 深=%d != 深[idom=%d]+1' % (a, c, b))
        x, step = a, 0
        while x != dom.get(x) and step <= n:
            if x not in dom:
                bad.append('支[%d] idom 链断' % a)
                break
            x = dom[x]
            step += 1
        else:
            if dom.get(x) != x:
                bad.append('支[%d] idom 链成环/仍非根' % a)
            elif fn.get(x) != d:
                bad.append('支[%d] 链到 别的函数 根' % a)
    # 每个函数首块 必须存在且是根
    for bid, fid in heads.items():
        if bid not in dom:
            bad.append('函数[%d] 首块=%d 无支配行' % (fid, bid))
        elif dom[bid] != bid:
            bad.append('函数[%d] 首块=%d 不是根 (idom=%d)' % (fid, bid, dom[bid]))
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tmp_dir('qcc_irdom')
    bad = []

    for name in ('kw3_a', 'kw3_b'):
        src = os.path.join(PROBE, name + '.c')
        rc, out = run([cc, src, '-o', os.path.join(w, name + '.exe')], w,
                      env={'QCC_DUMP': 'ir', 'QCC_RT_PATH': os.path.join(REPO, 'srclib', 'qcc_rt.c')})
        if rc != 0:
            print('[RED] %s 编译失败 rc=%d' % (name, rc))
            bad.append(name + '-compile')
            continue
        fails = verify(out)
        print('%s %s 支配森林 | %s' % ('PASS' if not fails else 'FAIL', name,
                                      '绿' if not fails else '; '.join(fails[:2])))
        if fails:
            bad.append(name)

    rc, out = run([cc, os.path.join(PROBE, 'kw3_a.c'), '-o', os.path.join(w, 'neg.exe')], w,
                  env={'QCC_DUMP': 'ir', 'QCC_RT_PATH': os.path.join(REPO, 'srclib', 'qcc_rt.c')})
    text = out
    m = PAT.search(text)
    if not m:
        bad.append('neg-setup')
    else:
        n = int(HDR.search(text).group(1))
        tang = text[:m.start()] + '支[%s] idom=%d 深=%s 函号=%s' % (
            m.group(1), n + 9, m.group(3), m.group(4)) + text[m.end():]
        ok = bool(verify(tang))
        print('%s 负例：idom 越界 -> verifier %s' % ('PASS' if ok else 'FAIL', '判红' if ok else '仍绿'))
        if not ok:
            bad.append('neg-green')

    # 负例 2：成环（支[1] idom=2，支[2] idom=1）
    rows = PAT.findall(text)
    if len(rows) >= 3:
        t2 = re.sub(r'支\[1\] idom=-?\d+ 深=-?\d+ 函号=-?\d+', '支[1] idom=2 深=1 函号=0', text, count=1)
        t2 = re.sub(r'支\[2\] idom=-?\d+ 深=-?\d+ 函号=-?\d+', '支[2] idom=1 深=2 函号=0', t2, count=1)
        ok2 = bool(verify(t2))
        print('%s 负例：idom 成环 -> verifier %s' % ('PASS' if ok2 else 'FAIL', '判红' if ok2 else '仍绿'))
        if not ok2:
            bad.append('neg-cycle')

    # 负例 3：跨函数 idom（找一条 idom!=自身 的行，把 idom 指到别的函数块）
    rows2 = [(int(a), int(b), int(c), int(d)) for a, b, c, d in PAT.findall(text)]
    pick = next(((a, b, c, d) for a, b, c, d in rows2 if b != a), None)
    other = next((a for a, b, c, d in rows2 if pick and d != pick[3]), None)
    if pick and other is not None:
        line = '  支[%d] idom=%d 深=%d 函号=%d' % (pick[0], pick[1], pick[2], pick[3])
        t3 = text.replace(line, '  支[%d] idom=%d 深=1 函号=%d' % (pick[0], other, pick[3]), 1)
        ok3 = bool(verify(t3))
        print('%s 负例：idom 跨函数 -> verifier %s' % ('PASS' if ok3 else 'FAIL', '判红' if ok3 else '仍绿'))
        if not ok3:
            bad.append('neg-fn')
    else:
        bad.append('neg-fn-setup')

    rc1, _ = run([cc, os.path.join(PROBE, 'kw3_a.c'), '-o', os.path.join(w, 'p1.exe')], w,
                 env={'QCC_RT_PATH': os.path.join(REPO, 'srclib', 'qcc_rt.c')})
    rc2, _ = run([cc, os.path.join(PROBE, 'kw3_a.c'), '-o', os.path.join(w, 'p2.exe')], w,
                 env={'QCC_RT_PATH': os.path.join(REPO, 'srclib', 'qcc_rt.c'), 'QCC_DUMP': 'ir'})
    if rc1 != 0 or rc2 != 0:
        bad.append('product-compile')
    else:
        ok = sha(os.path.join(w, 'p1.exe')) == sha(os.path.join(w, 'p2.exe'))
        print('%s 零产物影响（dump 不改 SHA）' % ('PASS' if ok else 'FAIL'))
        if not ok:
            bad.append('product')

    shutil.rmtree(w, ignore_errors=True)
    print('== 支配森林判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
