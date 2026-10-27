#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ir_meta —— IR→优化器 中间态 元数据 完整性硬闸（R880）。

断言五件事（全部 由 shipped/dev 编译器 真跑出来，不读源码旁证）：
  1. `-S` 产物 末尾 有 `; @QCCMETA 1 …` 附录，且 **覆盖率 100%**：每条 4-空格 指令行 恰有 一条 `@I`
  2. **无悬挂**：`@I` 的 节点 ∈ [0,nodes) · 类型码 k∈[0,31] · t>=-1 · 循环 ∈ [-1,loops]；
     `@U` 的 序 与 每个 使用者 都 ∈ [0,ins)；`@L` 的 1<=id<=loops、0<=s<=e<ins、d>=1
  3. **负控**：把 附录里 一条 `@I` 删掉 ⟹ 校验器 必须 判红（证明 校验器 不是 恒绿）
  4. **默认路径 零污染**：同一份源 `-c` / `-o` 产物 **不含** 任何 `@QCCMETA` 标记
  5. **逐字节 回归**：`-c` / `-o` 产物 与 改动前 归档的 对照编译器 **逐字节相同**

用法: python scripts/check_ir_meta.py [--cc <exe>] [--control <control.exe>] [--work <dir>]
退出码: 0 = 五条全过；1 = 有红；2 = 环境未就绪
"""
import argparse
import hashlib
import io
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

# 对照编译器 = **R880 改动前** 的 shipped（R879 收口 `1C153876`）。
# 判据 **fail-closed**：文件缺失 / 哈希不符 ⟹ 判红（不许「无对照 也 PASS」）。
CONTROL_SHA8 = '1C153876'

SRC = '''#include <stdio.h>
int 算和(int n) {
    int s = 0;
    for (int i = 0; i < n; i = i + 1) s = s + i;
    return s;
}
int 算积(int n) {
    if (n < 2) return 1;
    return n * 算积(n - 1);
}
int main(void) {
    int a = 算和(10);
    int b = 算积(5);
    printf("%d %d\\n", a, b);
    return 0;
}
'''


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, **kw)


def check_meta(text, verbose=True):
    """校验 元数据 附录；返 (ok, 消息列表)。"""
    lines = text.split('\n')
    hdr = None
    for l in lines:
        if l.startswith('; @QCCMETA 1 '):
            hdr = l
            break
    if hdr is None:
        return False, ['缺 @QCCMETA 头行']
    def num(key):
        m = re.search(r'\b%s=(-?\d+)' % key, hdr)
        return int(m.group(1)) if m else None
    ins, nodes, loops = num('ins'), num('nodes'), num('loops')
    if ins is None or nodes is None or loops is None:
        return False, ['头行缺 ins/nodes/loops: %s' % hdr]

    bad = []
    # 指令行 计数
    body = []
    for l in lines:
        if l.startswith('; @'):
            break
        body.append(l)
    inst = [i for i, l in enumerate(body) if l.startswith('    ') and not l.strip().startswith(';')]
    if len(inst) != ins:
        bad.append('指令行 %d ≠ 头行 ins=%d' % (len(inst), ins))

    I = {}
    U = {}
    L = {}
    for l in lines:
        if l.startswith('; @I '):
            m = re.match(r'; @I (\d+) f=(-?\d+) n=(-?\d+) k=(-?\d+) t=(-?\d+) l=(-?\d+) d=(-?\d+)$', l)
            if not m:
                bad.append('@I 格式 破损: %r' % l[:60]); continue
            i = int(m.group(1))
            if i in I:
                bad.append('@I 重复: %d' % i)
            I[i] = tuple(int(x) for x in m.groups()[1:])
        elif l.startswith('; @U '):
            m = re.match(r'; @U (\d+) r=(-?\d+) z=(-?\d+) u=([\d,]*)$', l)
            if not m:
                bad.append('@U 格式 破损: %r' % l[:60]); continue
            i = int(m.group(1))
            us = [int(x) for x in m.group(4).split(',') if x]
            U[i] = (int(m.group(2)), int(m.group(3)), us)
        elif l.startswith('; @L '):
            m = re.match(r'; @L (\d+) s=(-?\d+) e=(-?\d+) d=(-?\d+)$', l)
            if not m:
                bad.append('@L 格式 破损: %r' % l[:60]); continue
            L[int(m.group(1))] = tuple(int(x) for x in m.groups()[1:])

    missing = [i for i in range(ins) if i not in I]
    if missing:
        bad.append('覆盖率 缺口: %d 条 指令 无 @I（前 5: %s）' % (len(missing), missing[:5]))
    for i, (f, n, k, t, l, d) in I.items():
        if i < 0 or i >= ins:
            bad.append('@I 序 越界: %d' % i)
        if not (0 <= n < nodes):
            bad.append('@I 节点 悬挂: 指令 %d n=%d（nodes=%d）' % (i, n, nodes))
        if not (0 <= k <= 31):
            bad.append('@I 类型码 越界: 指令 %d k=%d' % (i, k))
        if t < -1:
            bad.append('@I 构序 越界: 指令 %d t=%d' % (i, t))
        if not (-1 <= l <= loops):
            bad.append('@I 循环 越界: 指令 %d l=%d' % (i, l))
        if d < 0:
            bad.append('@I 深度 越界: 指令 %d d=%d' % (i, d))
    for i, (r, z, us) in U.items():
        if i < 0 or i >= ins:
            bad.append('@U 序 越界: %d' % i)
        if not (0 <= r <= 15):
            bad.append('@U 寄存器 越界: 指令 %d r=%d' % (i, r))
        if z not in (0, 1):
            bad.append('@U z 非法: 指令 %d z=%d' % (i, z))
        for u in us:
            if u <= i or u >= ins:
                bad.append('@U 使用者 悬挂: 指令 %d → %d' % (i, u))
    for lid, (s, e, d) in L.items():
        if not (1 <= lid <= loops):
            bad.append('@L id 越界: %d' % lid)
        if not (0 <= s <= e < ins):
            bad.append('@L 区间 非良构: id=%d s=%d e=%d' % (lid, s, e))
        if d < 1:
            bad.append('@L 深度 非法: id=%d d=%d' % (lid, d))
    ok = not bad
    if verbose:
        if ok:
            print('  元数据: ins=%d nodes=%d loops=%d 覆盖 %d/%d · @U %d · @L %d ⟹ 良构'
                  % (ins, nodes, loops, len(I), ins, len(U), len(L)))
        else:
            for b in bad[:8]:
                print('  ✗ ' + b)
    return ok, bad


def strip_meta(text):
    return '\n'.join(l for l in text.split('\n') if not l.startswith('; @'))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--control', default=os.path.join(ROOT, 'build', 'control_qcc_1C153876.exe'))
    ap.add_argument('--work', default=os.path.join(os.environ.get('TEMP', '.'), 'qcc_ir_meta'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    ctrl = a.control if os.path.isabs(a.control) else os.path.join(ROOT, a.control)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: %s' % cc); return 2
    if not os.path.exists(ctrl):
        print('[FAIL] 对照编译器 缺失（fail-closed）: %s' % ctrl); return 1
    csha = hashlib.sha256(io.open(ctrl, 'rb').read()).hexdigest()[:8].upper()
    print('对照编译器: %s  sha256[:8]=%s（要求 %s）' % (os.path.basename(ctrl), csha, CONTROL_SHA8))
    if csha != CONTROL_SHA8:
        print('[FAIL] 对照编译器 哈希不符（fail-closed）：%s != %s' % (csha, CONTROL_SHA8)); return 1
    os.makedirs(a.work, exist_ok=True)
    src = os.path.join(a.work, 'meta_src.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
    fails = []

    print('== IR 元数据 完整性 (cc=%s) ==' % os.path.basename(cc))
    out = os.path.join(a.work, 'meta.sample')
    r = run([cc, '-S', src, '-o', out + '.exe'])
    if r.returncode != 0:
        print('  ✗ -S 失败 rc=%d' % r.returncode); return 1
    asm = out + '.exe.asm'
    text = io.open(asm, encoding='utf-8', errors='replace').read()
    ok, _ = check_meta(text, verbose=True)
    if not ok:
        fails.append('元数据 良构')

    # 负控：删掉一条 @I
    idx = text.index('; @I ')
    j = text.index('\n', idx)
    broken = text[:idx] + text[j + 1:]
    ok2, bad2 = check_meta(broken, verbose=False)
    print('  负控（删一条 @I）: %s' % ('判红 ✓' if not ok2 else '仍绿 ✗（校验器 恒绿）'))
    if ok2:
        fails.append('负控 未判红')

    # 默认路径 零污染 + 逐字节 回归
    tags = []
    for name, args in (('c', ['-c']), ('o', [])):
        p_new = os.path.join(a.work, 'new_%s.o' % name)
        p_old = os.path.join(a.work, 'old_%s.o' % name)
        r1 = run([cc] + (args if args else []) + [src, '-o', p_new])
        r2 = run([ctrl] + (args if args else []) + [src, '-o', p_old])
        if r1.returncode != 0:
            fails.append('%s 编译 失败' % name); continue
        blob = io.open(p_new, 'rb').read()
        if b'@QCCMETA' in blob:
            fails.append('%s 产物 含 元数据 标记' % name)
        if r2.returncode != 0:
            fails.append('%s: 对照编译器 编译失败 rc=%d' % (name, r2.returncode)); continue
        if r2.returncode == 0:
            same = hashlib.sha256(blob).hexdigest() == hashlib.sha256(io.open(p_old, 'rb').read()).hexdigest()
            tags.append('%s=%s' % (name, '同' if same else '异'))
            if not same:
                fails.append('%s 产物 与 对照 编译器 不 逐字节 相同' % name)
        else:
            tags.append('%s=无对照' % name)
    print('  默认路径 零污染 + 逐字节: %s' % ' '.join(tags))

    print('=' * 60)
    if fails:
        print('元数据 闸: FAIL -> %s' % ', '.join(fails))
        return 1
    print('元数据 闸: PASS（覆盖 100% · 悬挂 0 · 负控 判红 · -c/-o 零污染 且 与对照 逐字节相同）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
