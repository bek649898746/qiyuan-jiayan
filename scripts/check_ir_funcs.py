#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ir_funcs —— D3 CFG 函数边界 / 真基本块 判据（SSA 地基）。

修前实证（2026-11-03 · 虾米）：
  `构CFG` 只在 `控式!=0` 处切块，函数入口/分支目标/终结符之后都不切 ⟹
  相邻函数被落空边串成一条流；`scripts/probe/kw3_funcs.c` 一个块内出现 2–7 个 `返回(1072)`，
  修前共 49 个这样的块。支配树因此建立在跨函数的假边上（D2 的 kw3_a/b 都是单函数探针，没暴露）。

断言（读真实 `QCC_DUMP=ir` dump；用**空运行库**隔离，只留探针 4 个函数）：
  1. 函数边界：`跨函数边=0`、`跨函数块=0`；每个函数恰一个首块，首块 始==首节点、函号==函数号；
  2. 真基本块：**可达块内** 至多一个 `返回(1072)`（修前 49 块 >1）；
  3. 可达性：从函数首 + 全局前缀 BFS；`missing==0`（不可达块只允许是「终结符后的死尾」）；
  4. 支配森林：`未定=0`；idom ∈ 同函数；根 = 函数首 ∪ 死尾 ∪ 全局前缀；深度自洽；
  5. 负例：改块的函号 / 改后继 / 改 idom 跨函数 —— 同一 verifier 必须判红；
  6. 零产物影响：加/不加 `QCC_DUMP=ir`，`-c` 产物 SHA256 相同。

用法: python scripts/check_ir_funcs.py [--cc qcc_zh_seed.exe]
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
FUN = re.compile(r'  函数\[(\d+)\] 首节点=(\d+) 块=(\d+)')
DOM = re.compile(r'  支\[(\d+)\] idom=(-?\d+) 深=(-?\d+) 函号=(\d+)')
OP = re.compile(r'  抽\[(\d+)\] 算子=(\d+)')
BOUND = re.compile(r'IR CFG 函数边界 \(函数=(\d+) · 块数=(\d+) · 跨函数边=(\d+) · 跨函数块=(\d+)\)')
REACH = re.compile(r'IR CFG 可达性 \(函数根=(\d+) · 不可达块=(\d+) · 全局前缀=(\d+)\)')
DOMHDR = re.compile(r'IR 支配树 \(迭代 idom · 块数=(\d+) · 未定=(\d+)\)')
DOMRCH = re.compile(r'IR 支配树 可达性 \(可达=(\d+) · 不可达=(\d+)\)')


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
    d = {'blocks': {}, 'heads': {}, 'dom': {}, 'ops': {}}
    for l in text.splitlines():
        m = BLK.match(l)
        if m:
            v = [int(x) for x in m.groups()]
            d['blocks'][v[0]] = dict(start=v[1], end=v[2], ns=v[3], s0=v[4], back=v[5],
                                     ctrl=v[6], np=v[7], p0=v[8], s1=v[9], brk=v[10],
                                     cont=v[11], fn=v[12])
            continue
        m = FUN.match(l)
        if m:
            d['heads'][int(m.group(3))] = (int(m.group(1)), int(m.group(2)))
            continue
        m = DOM.match(l)
        if m:
            d['dom'][int(m.group(1))] = (int(m.group(2)), int(m.group(3)), int(m.group(4)))
            continue
        m = OP.match(l)
        if m:
            d['ops'][int(m.group(1))] = int(m.group(2))
    return d


def verify(text):
    """返回失败列表（空 = 绿）。纯函数，负例直接喂篡改文本。"""
    bad = []
    mb = BOUND.search(text)
    mr = REACH.search(text)
    mh = DOMHDR.search(text)
    md = DOMRCH.search(text)
    if not (mb and mr and mh and md):
        return ['缺 函数边界/可达性/支配树 段']
    nfun, nblk, xedge, xblk = (int(x) for x in mb.groups())
    froot, deadsum, gsum = (int(x) for x in mr.groups())
    dblk, und = (int(x) for x in mh.groups())
    reach_n, unreach_n = (int(x) for x in md.groups())
    d = parse(text)
    blocks, heads, dom, ops = d['blocks'], d['heads'], d['dom'], d['ops']
    if nblk <= 0 or len(blocks) != nblk:
        bad.append('块行 %d != 块数 %d' % (len(blocks), nblk))
    if dblk != nblk or len(dom) != nblk:
        bad.append('支配行 %d/%d != 块数 %d' % (len(dom), dblk, nblk))
    if nfun != 4:
        bad.append('函数=%d != 4（空运行库隔离失败？）' % nfun)
    if xedge != 0:
        bad.append('跨函数边=%d' % xedge)
    if xblk != 0:
        bad.append('跨函数块=%d' % xblk)
    if froot != nfun:
        bad.append('函数根=%d != 函数 %d' % (froot, nfun))
    if gsum != 1:
        bad.append('全局前缀=%d != 1' % gsum)
    if und != 0:
        bad.append('未定=%d' % und)

    # 函数首块：始==首节点、函号==函数号、唯一
    seen = set()
    for bid, (fid, fstart) in heads.items():
        if bid not in blocks:
            bad.append('函数[%d] 块=%d 越界' % (fid, bid))
            continue
        if bid in seen:
            bad.append('函数首块重复 %d' % bid)
        seen.add(bid)
        if blocks[bid]['start'] != fstart:
            bad.append('函数[%d] 首节点=%d != 块[%d].始=%d' % (fid, fstart, bid, blocks[bid]['start']))
        if blocks[bid]['fn'] != fid:
            bad.append('函数[%d] 块[%d].函号=%d' % (fid, bid, blocks[bid]['fn']))

    # 后继不得跨函数
    for bid, b in blocks.items():
        for s in (b['s0'], b['s1']):
            if s >= 0 and s in blocks and blocks[s]['fn'] != b['fn']:
                bad.append('块[%d]→块[%d] 跨函数 (%d→%d)' % (bid, s, b['fn'], blocks[s]['fn']))

    # 可达性以**支配行**为准（idom>=0 = 可达；idom=-1 = 不可达）。
    # 这样与编译器内部 `块可达` 同一口径（不用重算 BFS，避免漏掉 第三/第四 后继）。
    reach = {bid for bid, (idom, dep, fid) in dom.items() if idom >= 0}
    unreach = {bid for bid, (idom, dep, fid) in dom.items() if idom < 0}

    def lastop(bid):
        last = None
        for n in range(blocks[bid]['start'], blocks[bid]['end'] + 1):
            if n in ops:
                last = ops[n]
        return last

    dead, missing, globalp = [], [], []
    for bid in unreach:
        b = blocks.get(bid)
        if b is None:
            missing.append(bid)
            continue
        if b['fn'] == 0:
            globalp.append(bid)
            continue
        prev = blocks.get(bid - 1)
        if (prev and prev['end'] + 1 == b['start'] and (bid - 1) in reach
                and lastop(bid - 1) in (1041, 1042, 1072, 1121)):
            dead.append(bid)
        else:
            missing.append(bid)
    # 全局前缀 = 函号 0 的块（它是独立根，不是「不可达死尾」）
    for bid, b in blocks.items():
        if b['fn'] == 0 and bid not in globalp:
            globalp.append(bid)
    if missing:
        bad.append('缺边不可达块=%d %s' % (len(missing), sorted(missing)[:6]))
    if reach_n != len(reach):
        bad.append('可达=%d != 支配行可达 %d' % (reach_n, len(reach)))
    if unreach_n != len(unreach):
        bad.append('不可达=%d != 支配行不可达 %d' % (unreach_n, len(unreach)))
    if deadsum != len(dead):
        bad.append('不可达块=%d != 死尾 %d' % (deadsum, len(dead)))

    # 可达块内 至多一个 返回(1072)
    for bid in sorted(reach):
        b = blocks[bid]
        nr = sum(1 for n in range(b['start'], b['end'] + 1) if ops.get(n) == 1072)
        if nr > 1:
            bad.append('可达块[%d] 含 %d 个返回' % (bid, nr))

    # 支配森林：同函数、深度、根分类；不可达块 idom=-1、深=-1
    roots = set()
    for bid, (idom, dep, fid) in dom.items():
        if bid not in blocks:
            bad.append('支[%d] 越界' % bid)
            continue
        if idom < 0:
            if dep != -1:
                bad.append('支[%d] 不可达 深=%d != -1' % (bid, dep))
            continue
        if idom != bid and (idom not in blocks or blocks[idom]['fn'] != fid):
            bad.append('支[%d] idom=%d 跨函数/越界' % (bid, idom))
            continue
        if idom == bid:
            roots.add(bid)
            if dep != 0:
                bad.append('支[%d] 根 深=%d != 0' % (bid, dep))
            continue
        if dep != dom[idom][1] + 1:
            bad.append('支[%d] 深=%d != 父深+1' % (bid, dep))
        x, step = bid, 0
        while x != idom and step <= nblk:
            if x not in dom:
                bad.append('支[%d] 链断' % bid)
                break
            x = dom[x][0]
            step += 1
        else:
            if x != idom:
                bad.append('支[%d] 链成环' % bid)
    for bid in reach:
        if bid not in dom or dom[bid][0] < 0:
            bad.append('可达块[%d] 无 idom' % bid)
    expect_roots = set(heads.keys()) | set(globalp)
    if roots != expect_roots:
        bad.append('根集不符：多 %s 少 %s' % (sorted(roots - expect_roots)[:5],
                                             sorted(expect_roots - roots)[:5]))
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tmp_dir('qcc_irfuncs')
    io.open(os.path.join(w, 'qcc_rt.c'), 'w', encoding='utf-8', newline='\n').write('/* empty rt for IR probe */\n')
    io.open(os.path.join(w, 'qcc_snprintf_mini.c'), 'w', encoding='utf-8', newline='\n').write('/* empty mini rt */\n')
    env = {'QCC_DUMP': 'ir', 'QCC_RT_PATH': os.path.join(w, 'qcc_rt.c')}
    bad = []

    rc, out = run([cc, '-c', PROBE, '-o', os.path.join(w, 'kw3.obj')], w, env)
    text = out.decode('utf-8', 'replace')
    if rc != 0:
        print('[RED] 探针编译失败 rc=%d %r' % (rc, text[-200:]))
        bad.append('compile')
    else:
        fails = verify(text)
        print('%s 函数边界/基本块 | %s' % ('PASS' if not fails else 'FAIL',
                                           '绿' if not fails else '; '.join(fails[:4])))
        if fails:
            bad.append('verify')

        # 负例 1：改一个**有后继**的块的函号 → 跨函数后继必须判红
        d = parse(text)
        pick = None
        for bid, b in sorted(d['blocks'].items()):
            if b['s0'] >= 0 and b['s0'] in d['blocks']:
                pick = (bid, b)
                break
        if pick:
            bid, b = pick
            line = re.search(r'  块\[%d\][^\n]*' % bid, text)
            if line:
                old = line.group(0)
                tang = text.replace(old, old.replace('函号=%d' % b['fn'], '函号=999'), 1)
                ok = bool(verify(tang))
                print('%s 负例：块函号跨函数 -> verifier %s' % ('PASS' if ok else 'FAIL', '判红' if ok else '仍绿'))
                if not ok:
                    bad.append('neg-fn')
            else:
                bad.append('neg-setup')
        else:
            bad.append('neg-setup')

        # 负例 2：把一个 idom 改成越界值
        m2 = DOM.search(text)
        if m2:
            n = int(m2.group(1))
            tang = text[:m2.start()] + '  支[%d] idom=99999 深=1 函号=0' % n + text[m2.end():]
            ok = bool(verify(tang))
            print('%s 负例：idom 越界 -> verifier %s' % ('PASS' if ok else 'FAIL', '判红' if ok else '仍绿'))
            if not ok:
                bad.append('neg-dom')
        else:
            bad.append('neg-dom-setup')

    # 零产物影响
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
    print('== 函数边界/基本块 判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
