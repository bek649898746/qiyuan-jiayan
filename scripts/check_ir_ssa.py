#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ir_ssa —— D4b SSA 3C 判据（L0 真访问 → DF 布 φ → 定义支配校验）。

断言（读真实 `QCC_DUMP=ir` dump；空运行库隔离探针）：
  1. `IR SSA` 段存在；`溢出=0`、`缺定义=0`；
  2. 独立从 `L0访` 记录重建 def/use 键（帧 `基=1`、宽 1..8、键 = 函+偏；取址键排除），
     与 dump 的 `SSA键` 行逐键集合一致；
  3. 独立用支配父 + DF + 前驱表复算 iterated DF，φ 集合必须逐键相等；
  4. 每个 use 的 idom 链上必须出现同键的 def 或 φ；
  5. 负例：删一条 φ / 塞一条伪 φ / 把缺定义改成 1 —— 同一 verifier 必须判红；
  6. 零产物影响：加/不加 `QCC_DUMP=ir`，`-c` 产物 SHA256 相同。

用法: python scripts/check_ir_ssa.py [--cc qcc_zh_seed.exe]
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
DFL = re.compile(r'  支\[(\d+)\] DF=\[([^\]]*)\]')
PRED = re.compile(r'  前驱\[(\d+)\]((?: -?\d+)+)')
FUN = re.compile(r'  函数\[(\d+)\] 首节点=(\d+) 块=(\d+)')
L0 = re.compile(r'  L0访\[(\d+)\] 类=(\d+) 基=(\d+) 偏=(-?\d+) 宽=(\d+) 帧=(-?\d+) 槽=(-?\d+) 函=(\d+) 节=(-?\d+) 值=(-?\d+)')
SSAH = re.compile(r'IR SSA \(键=(\d+) · 定=(\d+) · 用=(\d+) · φ=(\d+) · 缺定义=(\d+) · 无定=(\d+) · 取址=(\d+) · 溢出=(\d+)\)')
SSAK = re.compile(r'  SSA键 函=(\d+) 基=(\d+) 偏=(-?\d+) 宽=(\d+) 取=(\d+) 参=(\d+) 定=\[([^\]]*)\] 用=\[([^\]]*)\] 法=\[([^\]]*)\]')


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


def ints(s):
    s = s.strip()
    return [int(x) for x in s.split(',')] if s else []


def parse(text):
    d = dict(blocks={}, dom={}, preds={}, df={}, heads={}, l0=[], keys={})
    for l in text.splitlines():
        m = BLK.match(l)
        if m:
            v = [int(x) for x in m.groups()]
            d['blocks'][v[0]] = dict(start=v[1], end=v[2], fn=v[12])
            continue
        m = DOM.match(l)
        if m:
            d['dom'][int(m.group(1))] = int(m.group(2))
            continue
        m = DFL.match(l)
        if m:
            d['df'][int(m.group(1))] = ints(m.group(2))
            continue
        m = PRED.match(l)
        if m:
            d['preds'][int(m.group(1))] = [int(x) for x in m.group(2).split()]
            continue
        m = FUN.match(l)
        if m:
            d['heads'][int(m.group(1))] = int(m.group(3))
            continue
        m = L0.match(l)
        if m:
            v = [int(x) for x in m.groups()]
            d['l0'].append(dict(cls=v[1], base=v[2], off=v[3], width=v[4], fn=v[7], node=v[8], value=v[9]))
            continue
        m = SSAK.match(l)
        if m:
            d['keys'][(int(m.group(1)), int(m.group(3)))] = dict(
                fn=int(m.group(1)), off=int(m.group(3)), width=int(m.group(4)),
                addr=int(m.group(5)), param=int(m.group(6)), defs=ints(m.group(7)),
                uses=ints(m.group(8)), phi=ints(m.group(9)))
    return d


def verify(text):
    bad = []
    mh = SSAH.search(text)
    if not mh:
        return ['缺 IR SSA 段']
    nkey, ndef, nuse, nphi, nmiss, nnod, naddr, novf = (int(x) for x in mh.groups())
    d = parse(text)
    blocks, dom, preds, df, heads = d['blocks'], d['dom'], d['preds'], d['df'], d['heads']
    reach = {b for b, idom in dom.items() if idom >= 0}
    if novf != 0:
        bad.append('溢出=%d' % novf)
    if nmiss != 0:
        bad.append('缺定义=%d' % nmiss)

    node_blk = {}
    for b, info in blocks.items():
        for n in range(info['start'], info['end'] + 1):
            node_blk[n] = b
    head_blk = {fn: b for fn, b in heads.items() if b in blocks}

    # 独立从 L0 重建
    comp = {}
    for r in d['l0']:
        if r['cls'] not in (1, 2, 3) or r['base'] != 1 or not (1 <= r['width'] <= 8):
            continue
        if r['value'] < 0:
            continue
        key = (r['fn'], r['off'])
        c = comp.setdefault(key, dict(defs=set(), uses=set(), addr=False, param=0))
        if d['keys'].get(key, {}).get('param'):
            c['param'] = 1
        b = node_blk.get(r['node'])
        if b is None or b not in reach or blocks[b]['fn'] != r['fn']:
            b = head_blk.get(r['fn'])
        if b is None or b not in reach:
            continue
        if c['param']:
            c['defs'].add(head_blk[r['fn']])
        if r['cls'] == 3:
            c['addr'] = True
        elif r['cls'] == 2:
            c['defs'].add(b)
        else:
            c['uses'].add(b)
    # 第二遍：值=-1 的直接帧存储（常量初始化/参数拷贝）补成已有键的 def
    for r in d['l0']:
        if r['cls'] != 2 or r['base'] != 1 or r['value'] >= 0 or not (1 <= r['width'] <= 8):
            continue
        key = (r['fn'], r['off'])
        c = comp.get(key)
        if not c:
            continue
        b = node_blk.get(r['node'])
        if b is None or b not in reach or blocks[b]['fn'] != r['fn']:
            b = head_blk.get(r['fn'])
        if b is not None and b in reach:
            c['defs'].add(b)

    # 对比 dump 键
    if len(d['keys']) != nkey:
        bad.append('SSA 键行 %d != 摘要 %d' % (len(d['keys']), nkey))
    for key, got in d['keys'].items():
        c = comp.get(key)
        if not c:
            bad.append('SSA 键 %s 不在 L0 重建中' % (key,))
            continue
        if set(got['defs']) != c['defs']:
            bad.append('键%s 定 %s != 重建 %s' % (key, sorted(got['defs'])[:6], sorted(c['defs'])[:6]))
        if set(got['uses']) != c['uses']:
            bad.append('键%s 用 %s != 重建 %s' % (key, sorted(got['uses'])[:6], sorted(c['uses'])[:6]))
    extra = set(comp) - set(d['keys'])
    if extra:
        bad.append('L0 重建有未导出键 %s' % sorted(extra)[:6])

    # 独立复算 iterated DF φ
    for key, c in comp.items():
        if c['addr'] or not c['defs']:
            continue
        work = list(c['defs'])
        gotphi = set()
        while work:
            x = work.pop()
            for y in df.get(x, []):
                ps = [p for p in preds.get(y, []) if p in reach]
                if y not in reach or len(ps) < 2 or (key, y) in gotphi:
                    continue
                gotphi.add((key, y))
                work.append(y)
        dump = set((key, b) for b in d['keys'].get(key, {}).get('phi', []))
        if gotphi != dump:
            bad.append('键%s φ 实算%s != 复算%s' % (key, sorted(b for _k, b in dump)[:6],
                                                  sorted(b for _k, b in gotphi)[:6]))

    # use-dominance（只对**有直接 def** 的键；无 def 的键单独计 `无定`，不冒充缺 φ）
    for key, c in comp.items():
        if c['addr'] or not c['defs']:
            continue
        defs = c['defs'] | set(d['keys'].get(key, {}).get('phi', []))
        for u in c['uses']:
            x, step, hit = u, 0, False
            while x in dom and step <= len(dom):
                if x in defs:
                    hit = True
                    break
                if dom[x] == x or dom[x] < 0:
                    break
                x = dom[x]
                step += 1
            if not hit:
                bad.append('键%s use@块%d 无支配 def/φ' % (key, u))
    nod = sum(1 for c in comp.values() if not c['addr'] and not c['defs'] and c['uses'])
    # 计数一致
    cdef = sum(len(c['defs']) for c in comp.values())
    cuse = sum(len(c['uses']) for c in comp.values())
    cphi = sum(len(v['phi']) for v in d['keys'].values())
    caddr = sum(1 for c in comp.values() if c['addr'])
    if ndef != cdef:
        bad.append('定=%d != 重建 %d' % (ndef, cdef))
    if nuse != cuse:
        bad.append('用=%d != 重建 %d' % (nuse, cuse))
    if nphi != cphi:
        bad.append('φ=%d != 行和 %d' % (nphi, cphi))
    if naddr != caddr:
        bad.append('取址=%d != 重建 %d' % (naddr, caddr))
    if nnod != nod:
        bad.append('无定=%d != 重建 %d' % (nnod, nod))
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tmp_dir('qcc_irssa')
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
        print('%s SSA 3C | %s' % ('PASS' if not fails else 'FAIL',
                                  '绿' if not fails else '; '.join(fails[:4])))
        if fails:
            bad.append('verify')

        # 负例 1：删一条 φ 行（找第一个非空 法= 的键）
        m = None
        for mm in SSAK.finditer(text):
            if mm.group(9).strip():
                m = mm
                break
        if m:
            body = m.group(9).split(',')
            tang = text[:m.start()] + m.group(0).replace(m.group(8), ','.join(body[1:]) if len(body) > 1 else '') + text[m.end():]
            ok = bool(verify(tang))
            print('%s 负例：删一条 φ -> verifier %s' % ('PASS' if ok else 'FAIL', '判红' if ok else '仍绿'))
            if not ok:
                bad.append('neg-del')
        else:
            bad.append('neg-del-setup')

        # 负例 2：塞一条伪 φ（给块 0）
        m0 = re.search(r'  SSA键 函=\d+ 基=\d+ 偏=-?\d+ 宽=\d+ 取=\d+ 参=\d+ 定=\[[^\]]*\] 用=\[[^\]]*\] 法=\[[^\]]*\]\n', text)
        if m0:
            line = m0.group(0)
            # 第一个 φ 列表若为空，填 0；否则追加 0
            mm = re.match(r'(.*法=\[)([^\]]*)(\].*\n)', line)
            newbody = (mm.group(2) + ',0') if mm.group(2).strip() else '0'
            tang = text[:m0.start()] + mm.group(1) + newbody + mm.group(3) + text[m0.end():]
            ok = bool(verify(tang))
            print('%s 负例：塞伪 φ -> verifier %s' % ('PASS' if ok else 'FAIL', '判红' if ok else '仍绿'))
            if not ok:
                bad.append('neg-add')
        else:
            bad.append('neg-add-setup')

        # 负例 3：把缺定义改成 1
        tang = re.sub(r'(IR SSA \(键=\d+ · 定=\d+ · 用=\d+ · φ=\d+ · 缺定义=)\d+', r'\g<1>1', text, count=1)
        ok = bool(verify(tang))
        print('%s 负例：缺定义=1 -> verifier %s' % ('PASS' if ok else 'FAIL', '判红' if ok else '仍绿'))
        if not ok:
            bad.append('neg-miss')

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
    print('== SSA 3C 判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
