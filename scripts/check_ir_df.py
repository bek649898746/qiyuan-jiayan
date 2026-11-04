#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ir_df —— D4 支配边界（DF）判据（SSA φ 插入的地基）。

断言（读真实 `QCC_DUMP=ir` dump；空运行库隔离探针）：
  1. `IR 支配边界` 段存在；块数 == CFG 块数；`溢出=0`；`DF边` == 逐块列表之和；
  2. 每条 (a→b) 的 b 是可达块且 **可达前驱 >= 2**；a 是可达块；
  3. 用支配父 + 全量前驱表 **独立复算** DF，集合必须逐块相等；
     定义：b ∈ DF(a) ⟺ a 支配 b 的某个前驱 p，且 a 不严格支配 b；
  4. 负例：删一条 DF 边 / 加一条伪 DF 边，同一 verifier 必须判红；
  5. 零产物影响：加/不加 `QCC_DUMP=ir`，`-c` 产物 SHA256 相同。

用法: python scripts/check_ir_df.py [--cc qcc_zh_seed.exe]
退出码: 0 = PASS / 1 = RED
"""
import argparse
import hashlib
import io
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROBE = os.path.join(REPO, 'scripts', 'probe', 'kw3_funcs.c')

BLK = re.compile(r'  块\[(\d+)\] 始=(-?\d+) 终一=(-?\d+) 后继数=(\d+) 后继0=(-?\d+) '
                 r'回边=(\d+) 首控式=(\d+) 前驱数=(\d+) 前驱0=(-?\d+) 后继1=(-?\d+) '
                 r'断目标=(-?\d+) 继续目标=(-?\d+) 函号=(\d+)')
DOM = re.compile(r'  支\[(\d+)\] idom=(-?\d+) 深=(-?\d+) 函号=(\d+)')
DFH = re.compile(r'IR 支配边界 \(块=(\d+) · DF边=(\d+) · 溢出=(\d+)\)')
DFL = re.compile(r'  支\[(\d+)\] DF=\[([^\]]*)\]')
PRED = re.compile(r'  前驱\[(\d+)\]((?: -?\d+)+)')


def run(args, cwd, env=None):
    e = dict(os.environ)
    if env:
        e.update(env)
    r = subprocess.run(args, cwd=cwd, capture_output=True, env=e)
    return r.returncode, (r.stdout or b'') + (r.stderr or b'')


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for c in iter(lambda: f.read(1 << 20), b''):
            h.update(c)
    return h.hexdigest()


def parse(text):
    d = {'blocks': {}, 'dom': {}, 'preds': {}, 'df': {}}
    for l in text.splitlines():
        m = BLK.match(l)
        if m:
            v = [int(x) for x in m.groups()]
            d['blocks'][v[0]] = dict(start=v[1], end=v[2], s0=v[4], s1=v[9], fn=v[12])
            continue
        m = DOM.match(l)
        if m:
            d['dom'][int(m.group(1))] = int(m.group(2))
            continue
        m = PRED.match(l)
        if m:
            d['preds'][int(m.group(1))] = [int(x) for x in m.group(2).split()]
            continue
        m = DFL.match(l)
        if m:
            body = m.group(2).strip()
            d['df'][int(m.group(1))] = [int(x) for x in body.split(',')] if body else []
    return d


def verify(text):
    bad = []
    mh = DFH.search(text)
    if not mh:
        return ['缺 支配边界 段']
    n, nedges, ovf = (int(x) for x in mh.groups())
    d = parse(text)
    blocks, dom, preds, df = d['blocks'], d['dom'], d['preds'], d['df']
    if n <= 0 or len(blocks) != n:
        bad.append('块行 %d != 块数 %d' % (len(blocks), n))
    if ovf != 0:
        bad.append('溢出=%d' % ovf)
    if nedges != sum(len(v) for v in df.values()):
        bad.append('DF边=%d != 列表和 %d' % (nedges, sum(len(v) for v in df.values())))
    reach = {b for b, idom in dom.items() if idom >= 0}

    # 逐块复算
    expect = {b: set() for b in blocks}
    for b in reach:
        ps = [p for p in preds.get(b, []) if p in reach]
        if len(ps) < 2:
            continue
        ib = dom[b]
        for p in ps:
            x, step = p, 0
            while x != ib and x >= 0 and step <= n:
                expect[x].add(b)
                if x not in dom:
                    break
                x = dom[x]
                step += 1
    for a, targets in df.items():
        if a not in blocks:
            bad.append('DF 源块[%d] 越界' % a)
            continue
        if a not in reach:
            bad.append('DF 源块[%d] 不可达' % a)
        for b in targets:
            if b not in reach:
                bad.append('DF 目标块[%d] 不可达' % b)
            if len([p for p in preds.get(b, []) if p in reach]) < 2:
                bad.append('DF 目标块[%d] 可达前驱 < 2' % b)
        if set(targets) != expect.get(a, set()):
            bad.append('DF[%d] 实算 %s != 复算 %s' % (a, sorted(set(targets))[:6], sorted(expect.get(a, set()))[:6]))
    for a, s in expect.items():
        if s and a not in df:
            bad.append('漏 DF[%d] %s' % (a, sorted(s)[:6]))
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tmp_dir('qcc_irdf')
    for fn, body in (('qcc_rt.c', '/* empty rt */\n'), ('qcc_snprintf_mini.c', '/* empty mini */\n')):
        io.open(os.path.join(w, fn), 'w', encoding='utf-8', newline='\n').write(body)
    env = {'QCC_DUMP': 'ir', 'QCC_RT_PATH': os.path.join(w, 'qcc_rt.c')}
    bad = []

    rc, out = run([cc, '-c', PROBE, '-o', os.path.join(w, 'p.obj')], w, env)
    text = out.decode('utf-8', 'replace')
    if rc != 0:
        print('[RED] 探针编译失败 rc=%d %r' % (rc, text[-200:]))
        bad.append('compile')
    else:
        fails = verify(text)
        print('%s 支配边界 | %s' % ('PASS' if not fails else 'FAIL',
                                    '绿' if not fails else '; '.join(fails[:4])))
        if fails:
            bad.append('verify')

        # 负例 1：删一条 DF 边
        m = DFL.search(text)
        if m and m.group(2).strip():
            body = m.group(2).split(',')
            tang = text[:m.start()] + m.group(0).replace(m.group(2), ','.join(body[1:]) if len(body) > 1 else '') + text[m.end():]
            ok = bool(verify(tang))
            print('%s 负例：删一条 DF 边 -> verifier %s' % ('PASS' if ok else 'FAIL', '判红' if ok else '仍绿'))
            if not ok:
                bad.append('neg-del')
        else:
            bad.append('neg-del-setup')

        # 负例 2：加一条伪 DF 边（给块 0 加一个 DF=0）
        m0 = re.search(r'  支\[0\] DF=\[[^\]]*\]\n', text)
        if m0:
            tang = text[:m0.start()] + '  支[0] DF=[0]\n' + text[m0.end():]
            ok = bool(verify(tang))
            print('%s 负例：加伪 DF 边 -> verifier %s' % ('PASS' if ok else 'FAIL', '判红' if ok else '仍绿'))
            if not ok:
                bad.append('neg-add')
        else:
            # 块 0 没有 DF 行：在 DF 段后补一行
            mh = DFH.search(text)
            tang = text[:mh.end()] + '\n  支[0] DF=[0]' + text[mh.end():]
            ok = bool(verify(tang))
            print('%s 负例：加伪 DF 边 -> verifier %s' % ('PASS' if ok else 'FAIL', '判红' if ok else '仍绿'))
            if not ok:
                bad.append('neg-add')

    rc1, _ = run([cc, '-c', PROBE, '-o', os.path.join(w, 'p1.obj')], w,
                 {'QCC_RT_PATH': os.path.join(w, 'qcc_rt.c')})
    rc2, _ = run([cc, '-c', PROBE, '-o', os.path.join(w, 'p2.obj')], w, env)
    if rc1 != 0 or rc2 != 0:
        bad.append('product-compile')
        print('[RED] 零产物影响：编译 rc=%d/%d' % (rc1, rc2))
    else:
        ok = sha(os.path.join(w, 'p1.obj')) == sha(os.path.join(w, 'p2.obj'))
        print('%s 零产物影响（dump 不改 SHA）' % ('PASS' if ok else 'FAIL'))
        if not ok:
            bad.append('product')

    shutil.rmtree(w, ignore_errors=True)
    print('== 支配边界判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
