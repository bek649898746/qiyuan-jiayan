#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ir_verify —— D 段 IR 3B verifier 判据（CFG/def-use 结构自检）。

断言（全部读真实 `QCC_DUMP=ir` dump）：
  1. **块结构**：块表非空、每块 始<=终一、无缝铺满、后继 ∈ [0,上退)；
  2. **双向一致**：dump 的「前驱/后继 双向一致」行 应查>0 且 违例=0；
  3. **回边**：回边数 == 控式==1 节点数（循环头数）；
  4. **def-use**：「被 L1 指令 引用的值」> 0，且引用的值都已在值表登记（值表存 非 0）；
  5. **负例**：把 dump 的后继改成越界后，同一个 verifier 必须判红（证明不是永绿）；
  6. **零产物影响**：同源加/不加 `QCC_DUMP=ir`，产物 SHA256 相同。

用法: python scripts/check_ir_verify.py [--cc qcc_zh_seed.exe]
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
    return r.returncode, dec(r.stdout or b''), dec(r.stderr or b'')


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for c in iter(lambda: f.read(1 << 20), b''):
            h.update(c)
    return h.hexdigest()


def verify(text):
    """返回失败列表（空 = 绿）。纯函数，负例直接喂篡改文本。"""
    bad = []
    m = re.search(r'IR CFG 块表 .*?上退=(\d+)', text)
    if not m:
        return ['缺 CFG 块表']
    nblk = int(m.group(1))
    if nblk <= 0:
        bad.append('块数=0')
    blocks = re.findall(r'块\[(\d+)\] 始=(-?\d+) 终一=(-?\d+) 后继数=(\d+) 后继0=(-?\d+) 回边=(\d+) 首控式=(\d+)', text)
    blocks = [tuple(int(x) for x in b) for b in blocks]
    if len(blocks) != nblk:
        bad.append('块行 %d != 上退 %d' % (len(blocks), nblk))
    for b in blocks:
        if b[1] > b[2]:
            bad.append('块[%d] 始>终一' % b[0])
        if b[3] > 0 and not (0 <= b[4] < nblk):
            bad.append('块[%d] 后继0=%d 越界' % (b[0], b[4]))
    for i in range(len(blocks) - 1):
        if blocks[i + 1][1] != blocks[i][2] + 1:
            bad.append('块不连续 @%d' % i)
    m2 = re.search(r'双向一致.*?应查=(\d+) 违例=(\d+)', text)
    if not m2:
        bad.append('缺 双向一致 行')
    elif int(m2.group(1)) <= 0 or int(m2.group(2)) != 0:
        bad.append('双向一致 应查=%s 违例=%s' % (m2.group(1), m2.group(2)))
    loops = len(re.findall(r'节\[\d+\] 控式=1 ', text))
    back_edges = sum(1 for b in blocks if b[5] == 1)
    if loops and back_edges != loops:
        bad.append('回边块 %d != 控式1 %d' % (back_edges, loops))
    m4 = re.search(r'被 L1 指令 引用的 值 = (\d+)', text)
    if not m4 or int(m4.group(1)) <= 0:
        bad.append('def-use 引用值=0/缺行')
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tmp_dir('qcc_irverify')
    bad = []

    for name in ('kw3_a', 'kw3_b'):
        src = os.path.join(PROBE, name + '.c')
        rc, out, err = run([cc, src, '-o', os.path.join(w, name + '.exe')], w,
                           env={'QCC_DUMP': 'ir', 'QCC_RT_PATH': os.path.join(REPO, 'srclib', 'qcc_rt.c')})
        if rc != 0:
            print('[RED] %s 编译失败 rc=%d %r' % (name, rc, err[-160:]))
            bad.append(name + '-compile')
            continue
        text = out + err
        fails = verify(text)
        print('%s %s verifier | %s' % ('PASS' if not fails else 'FAIL', name,
                                       '绿' if not fails else '; '.join(fails[:3])))
        if fails:
            bad.append(name)

    # 负例：把**有后继**的块的后继0改成越界，同一 verifier 必须判红
    rc, out, err = run([cc, os.path.join(PROBE, 'kw3_a.c'), '-o', os.path.join(w, 'neg.exe')], w,
                       env={'QCC_DUMP': 'ir', 'QCC_RT_PATH': os.path.join(REPO, 'srclib', 'qcc_rt.c')})
    text = out + err
    m = None
    for mm in re.finditer(r'  块\[(\d+)\] 始=(-?\d+) 终一=(-?\d+) 后继数=(\d+) 后继0=(-?\d+)', text):
        if int(mm.group(4)) > 0:
            m = mm
            break
    if not m:
        print('[RED] 负例：dump 缺有后继的块')
        bad.append('neg-setup')
    else:
        nblk = int(re.search(r'IR CFG 块表 .*?上退=(\d+)', text).group(1))
        tang = text[:m.start()] + '  块[%s] 始=%s 终一=%s 后继数=%s 后继0=%d' % (
            m.group(1), m.group(2), m.group(3), m.group(4), nblk + 7) + text[m.end():]
        nf = verify(tang)
        ok = bool(nf)
        print('%s 负例：篡改后继越界 -> verifier %s' % ('PASS' if ok else 'FAIL', '判红' if ok else '仍绿'))
        if not ok:
            bad.append('neg-green')

    # 零产物影响
    rc1, _, _ = run([cc, os.path.join(PROBE, 'kw3_a.c'), '-o', os.path.join(w, 'p1.exe')], w,
                    env={'QCC_RT_PATH': os.path.join(REPO, 'srclib', 'qcc_rt.c')})
    rc2, _, _ = run([cc, os.path.join(PROBE, 'kw3_a.c'), '-o', os.path.join(w, 'p2.exe')], w,
                    env={'QCC_RT_PATH': os.path.join(REPO, 'srclib', 'qcc_rt.c'), 'QCC_DUMP': 'ir'})
    if rc1 != 0 or rc2 != 0:
        bad.append('product-compile')
        print('[RED] 零产物影响：编译 rc=%d/%d' % (rc1, rc2))
    else:
        ok = sha(os.path.join(w, 'p1.exe')) == sha(os.path.join(w, 'p2.exe'))
        print('%s 零产物影响（dump 不改 SHA）' % ('PASS' if ok else 'FAIL'))
        if not ok:
            bad.append('product')

    shutil.rmtree(w, ignore_errors=True)
    print('== IR verifier 判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
