#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_opt_switches —— D5：8 条核心窥孔 **独立开关** 判据。

对象：`srclib/opt_zh.jy`（出厂优化器）。8 条核心窥孔：
  ① 栈spill ② 栈合并 ③ 抵消对 ④ 不可达(跳转+返回) ⑤ 复制传播
  ⑥ 跳落下一行 ⑦ 帧栈同址 ⑧ 死值写覆盖
开关：默认 255（连续三连绿后已默认开）；`--no-i` 只关闭第 i 条。

断言：
  1. 8 个**互不重叠**的最小触发输入：全开时仅目标类命中（其余 8 类 = 0）；
  2. `--no-i`：目标类 = 0，其余类与全开逐类相同；
  3. 三连跑（同一输入）：输出字节与逐类计数完全一致；
  4. 所有输出都能被 `asm_zh.exe` 汇编（rc=0）；
  5. 负例：`--no-i` 后目标类仍 >0 ⟹ 判红（证明不是永绿）。

用法: python scripts/check_opt_switches.py [--cc qcc_zh_seed.exe]
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
OPT_SRC = os.path.join(REPO, 'srclib', 'opt_zh.jy')
ASM = os.path.join(REPO, 'asm_zh.exe')

# 互不重叠的最小触发输入（每条只含目标类的形态）
CASES = [
    ('栈spill', '减栈 8\n    存栈64 [rsp+0], r0\n'),
    ('栈合并', '取栈64 r0, [rsp+0]\n    加栈 8\n'),
    ('抵消对', '加栈 24\n    减栈 24\n'),
    ('不可达', '跳转 .La\n    跳转 .Lb\n.La:\n.Lb:\n'),
    ('复制传播', '移64 r2, r3\n    移64 r4, r2\n    移64 r2, r1\n'),
    ('跳落到下一行', '跳转 .Ld\n.Ld:\n'),
    ('帧栈存取同址', '存帧64 [rbp-8], r0\n    取帧64 r3, [rbp-8]\n'),
    ('死值写覆盖', '移动 r0, 999\n    移64 r0, r1\n'),   # K>127 避开 ⑯ 立即数缩短（否则 ⑯ 先吃掉这行）
]
FIELDS = ['栈spill', '栈合并', '抵消对', '不可达', '复制传播', '跳落到下一行', '帧栈存取同址', '死值写覆盖']


def run(args, cwd=REPO):
    r = subprocess.run(args, cwd=cwd, capture_output=True)
    return r.returncode, (r.stdout or b'') + (r.stderr or b'')


def counts(text):
    t = text.decode('utf-8', 'replace')
    m = re.search(r'优化 \d+ 处 \[([^\]]*)\]', t)
    if not m:
        return None
    s = m.group(1)
    out = {}
    out['栈spill'] = int(re.search(r'栈spill (\d+)', s).group(1)) if re.search(r'栈spill (\d+)', s) else 0
    out['栈合并'] = int(re.search(r'栈合并 (\d+)', s).group(1)) if re.search(r'栈合并 (\d+)', s) else 0
    out['抵消对'] = int(re.search(r'抵消对 (\d+)', s).group(1)) if re.search(r'抵消对 (\d+)', s) else 0
    mj = re.search(r'不可达 跳转 (\d+) 返回 (\d+)', s)
    out['不可达'] = (int(mj.group(1)) + int(mj.group(2))) if mj else 0
    out['复制传播'] = int(re.search(r'复制传播 (\d+)', s).group(1)) if re.search(r'复制传播 (\d+)', s) else 0
    out['跳落到下一行'] = int(re.search(r'跳落到下一行 (\d+)', s).group(1)) if re.search(r'跳落到下一行 (\d+)', s) else 0
    out['帧栈存取同址'] = int(re.search(r'帧栈存取同址 (\d+)', s).group(1)) if re.search(r'帧栈存取同址 (\d+)', s) else 0
    out['死值写覆盖'] = int(re.search(r'死值写覆盖 (\d+)', s).group(1)) if re.search(r'死值写覆盖 (\d+)', s) else 0
    return out


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for c in iter(lambda: f.read(1 << 20), b''):
            h.update(c)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tmp_dir('qcc_optsw')
    opt = os.path.join(w, 'opt_zh.exe')
    rc, out = run([cc, OPT_SRC, '-o', opt], w)
    if rc != 0:
        print('[RED] opt_zh 编译失败 rc=%d %r' % (rc, out[-200:]))
        return 1
    bad = []
    base_counts = {}

    for i, (name, body) in enumerate(CASES, 1):
        src = os.path.join(w, 'c%d.asm' % i)
        io.open(src, 'w', encoding='utf-8', newline='\n').write('_入口:\n    ' + body + '    返回\n')
        allout = os.path.join(w, 'c%d_all.asm' % i)
        rc, out = run([opt, src, '-o', allout], w)
        c_all = counts(out)
        if rc != 0 or c_all is None:
            bad.append('%s 全开失败 rc=%d' % (name, rc))
            continue
        base_counts[name] = c_all
        # 目标类 >0，其余 8 类 = 0（互不重叠）
        if c_all.get(name, 0) <= 0:
            bad.append('%s 全开未命中' % name)
        for f in FIELDS:
            if f != name and c_all.get(f, 0) != 0:
                bad.append('%s 输入串到 %s=%d' % (name, f, c_all[f]))
        noout = os.path.join(w, 'c%d_no.asm' % i)
        rc2, out2 = run([opt, src, '--no-%d' % i, '-o', noout], w)
        c_no = counts(out2)
        if rc2 != 0 or c_no is None:
            bad.append('%s --no-%d 失败' % (name, i))
            continue
        if c_no.get(name, 0) != 0:
            bad.append('%s --no-%d 仍命中 %d' % (name, i, c_no[name]))
        for f in FIELDS:
            if f != name and c_no.get(f, 0) != c_all.get(f, 0):
                bad.append('%s --no-%d 误伤 %s (%d→%d)' % (name, i, f, c_all[f], c_no[f]))
        for p in (allout, noout):
            rc3, _ = run([ASM, p, '-o', p + '.exe'], w)
            if rc3 != 0:
                bad.append('%s 汇编失败 %s' % (name, os.path.basename(p)))

    # 三连跑：同一组合输入，字节与计数一致
    comb = os.path.join(w, 'comb.asm')
    io.open(comb, 'w', encoding='utf-8', newline='\n').write(
        '_入口:\n' + ''.join('    ' + b for _n, b in CASES) + '    返回\n')
    shas = []
    cts = []
    for r in range(3):
        o = os.path.join(w, 'comb_%d.asm' % r)
        rc, out = run([opt, comb, '-o', o], w)
        if rc != 0:
            bad.append('三连跑 第%d 失败 rc=%d' % (r + 1, rc))
            continue
        shas.append(sha(o))
        cts.append(counts(out))
    if len(set(shas)) != 1 or any(c != cts[0] for c in cts) or cts[0] is None:
        bad.append('三连跑 输出/计数不一致')

    # 打印现场
    for name, _b in CASES:
        c = base_counts.get(name, {})
        print('%-14s 全开 %s' % (name, {k: c.get(k, 0) for k in FIELDS if c.get(k, 0)}))
    print('%s 8 条独立开关 | %s' % ('PASS' if not bad else 'FAIL',
                                   '绿' if not bad else '; '.join(bad[:4])))
    shutil.rmtree(w, ignore_errors=True)
    print('== 优化器开关判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
