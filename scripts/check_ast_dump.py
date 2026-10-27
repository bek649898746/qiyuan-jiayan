#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ast_dump —— AST 侧表（`QCC_DUMP=ast`）**真接入**判据（2026-10-26 · D-AST 一致深）。

为什么存在：台账 D-AST 原结清口径是「AST 节点表/侧表已可用；此前『未启用』判定过期」，
证据只有一份历史 dump 日志（`_evidence/20261018_round7/debt_ast.log`）—— **没有可复跑判据**，
谁也无法当场证明这条通道现在仍然可用、且侧表与主表仍然同构。

本判据用 shipped 编译器编一个固定小源，断言四件事：
  (1) 通道可用：`QCC_DUMP=ast` 时 stdout/stderr 出现 `==== 抽象语法树 (根=… 节点数=… 顶层函数=…) ====`；
  (2) 结构正确：顶层函数 == 2（add / main），节点数 > 10，最大深度 >= 3；
  (3) **侧表同构**：每一行 `节[i]` 都满足 `节型 == 节型别`（两张平行表必须同时写、写同一个值）——
      这是「侧表」这类机制最容易腐坏的地方（只更新一张表，dump 看着正常但类型信息错位）；
  (4) **负控**：同一源不加 `QCC_DUMP` 编译时**不得**出现 AST 块（证明是显式开关，不是默认噪声）。

用法: python scripts/check_ast_dump.py [--cc qcc_zh_v1.exe]
退出码: 0 = 全过；1 = 有红；2 = 环境未就绪
"""
import argparse
import io
import os
import re
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SRC = '''#include <stdio.h>
int add(int a, int b) { return a + b; }
int main(void) { printf("%d\\n", add(2, 3)); return 0; }
'''

HEAD = re.compile(r'抽象语法树 \(根=(\d+) 节点数=(\d+) 顶层函数=(\d+)\)')
NODE = re.compile(r'^节\[(\d+)\] 深=(\d+) 节型=(-?\d+) 节型别=(-?\d+) 值=(-?\d+).*?名=(.*)$')


def run(cc, src, dump):
    env = dict(os.environ)
    if dump:
        env['QCC_DUMP'] = 'ast'
    else:
        env.pop('QCC_DUMP', None)
    obj = src[:-2] + ('.dump.o' if dump else '.plain.o')
    r = subprocess.run([cc, src, '-c', '-o', obj], capture_output=True, env=env, timeout=600)
    out = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    return r.returncode, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到 %s（本判据未执行，不判绿）' % cc)
        return 2

    w = tempfile.mkdtemp(prefix='qcc_ast_')
    src = os.path.join(w, 'ast.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)

    bad = []
    rc, out = run(cc, src, True)
    if rc != 0:
        print('[FAIL] 带 QCC_DUMP=ast 编译 rc=%d' % rc)
        print(out[-400:])
        return 1

    m = HEAD.search(out)
    if not m:
        print('[FAIL] 未出现 AST 头行（QCC_DUMP=ast 通道断了？）')
        print(out[-300:])
        return 1
    root, nodes, funcs = int(m.group(1)), int(m.group(2)), int(m.group(3))
    print('[通道] 根=%d 节点数=%d 顶层函数=%d' % (root, nodes, funcs))
    if root != 1:
        bad.append('根 != 1（%d）' % root)
    if funcs != 2:
        bad.append('顶层函数 != 2（%d；源里只有 add/main）' % funcs)
    if nodes <= 10:
        bad.append('节点数过少（%d）—— dump 可能被截断' % nodes)

    rows = 0
    maxd = 0
    names = set()
    for line in out.splitlines():
        mm = NODE.match(line.strip())
        if not mm:
            continue
        rows += 1
        depth = int(mm.group(2))
        t1, t2 = int(mm.group(3)), int(mm.group(4))
        maxd = max(maxd, depth)
        if t1 != t2:
            bad.append('侧表不同构: 节[%s] 节型=%d 节型別=%d' % (mm.group(1), t1, t2))
        if mm.group(6):
            names.add(mm.group(6).strip())
    print('[结构] 节点行=%d 最大深度=%d 名字样本=%s' % (rows, maxd, sorted(names)[:6]))
    if rows == 0:
        bad.append('一行 `节[i]` 都没解析到（格式变了？）')
    # R877：头行的「节点数」= 根可达数 ⟹ **必须等于实际印出的节点行数**（原报接子数，恒多 1；
    #   且函数定义循环会二次展开子树，实测 35 行 / 18 节点）。这条断言就是那次根治的护栏。
    if rows != nodes:
        bad.append('自洽性: 印出 %d 行 ≠ 头行节点数 %d（头行必须报根可达数）' % (rows, nodes))
    if len(names) and len(set(names)) > rows:
        bad.append('名字数 > 节点行数（不可能）')
    if maxd < 3:
        bad.append('最大深度 < 3（%d）—— 树被拍平或 dump 截断' % maxd)
    for want in ('add', 'main'):
        if want not in names:
            bad.append('名字表里缺 `%s`' % want)

    rc2, out2 = run(cc, src, False)
    if rc2 != 0:
        bad.append('不带 QCC_DUMP 的对照编不过（rc=%d）' % rc2)
    if '抽象语法树' in out2:
        bad.append('负控失败: 未设 QCC_DUMP 也输出了 AST（开关不是显式的）')
    print('[负控] 不带开关: rc=%d，AST 块=%s' % (rc2, '有（错）' if '抽象语法树' in out2 else '无（对）'))

    if bad:
        print('\n=== AST 侧表判据: FAIL ===')
        for b in bad:
            print('  ✗ ' + b)
        return 1
    print('\n=== AST 侧表判据: PASS（通道可用 · 结构正确 · 侧表同构 · 开关显式）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
