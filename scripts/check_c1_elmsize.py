#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""C1-ELMSIZE 判据 —— 同一取值链用**不同语法形态**访问，必须得到同一个值。

## 背景

台账 C1 记：「变量表缺『最内层元素尺寸』载体 ⟹ 表达式内重 **变量分支**（取 `指内`）与
**成员分支**（取 `主挂`）语义不一致」。若真不一致，同一块内存用两种写法读出的值会不同。

本判据把同一链的 4 种写法放在**同一个程序**里，全打印出来逐项比对：

| 写法 | 走的路径 |
|:--|:--|
| `o.arr[1].a`      | 成员基 |
| `p->arr[1].a`     | 变量基 + 箭头 |
| `(*p).arr[1].a`   | 解引用 + 成员基 |
| `q[0].arr[1].a`   | 指针下标 + 成员基（元素尺寸 = sizeof(构 外)）|

另加一组**双层数组**（`构 外 { 构 内 g[2][2]; }`）与一组**指向数组的指针**（`构 内 (*pa)[2]`），
覆盖 `指内/主挂` 两种元素尺寸来源。全部期望值由源里写死的字面量决定，不读实现细节。

用法：
    python scripts/check_c1_elmsize.py             # 默认 qcc_zh_v1.exe
    python scripts/check_c1_elmsize.py --cc qcc_pd.exe
退出码：0 = 全一致；1 = 有不一致；2 = 环境缺件。
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(ROOT, 'scratch_c1')

SRC = '''# 包含 <stdio.h>
构 内 { 整 a; 整 b; }
构 外 { 构 内 arr[3]; 构 内 g[2][2]; 整 t; }
函数 整 主(空) {
    构 外 o;
    构 外 *p = &o;
    构 内 (*pa)[2] = &o.g[0];
    o.arr[0].a = 10; o.arr[1].a = 20; o.arr[2].a = 30;
    o.arr[0].b = 1;  o.arr[1].b = 2;  o.arr[2].b = 3;
    o.g[0][1].a = 41; o.g[1][0].a = 42;
    o.g[0][1].b = 51; o.g[1][0].b = 52;
    o.t = 7;
    单 fa = 1.5f, fb = 2.5f;
    单 *池[2]; 池[0] = &fa; 池[1] = &fb;
    单 **q = 池;
    格式化 "CHAIN %d %d %d %d\\n", o.arr[1].a, p->arr[1].a, (*p).arr[1].a, p[0].arr[1].a;
    格式化 "DEEP  %d %d %d\\n", o.g[0][1].a, p->g[0][1].a, pa[0][1].a;
    格式化 "BFLD  %d %d %d\\n", o.arr[1].b, p->arr[1].b, (*p).arr[1].b;
    格式化 "TFLD  %d %d\\n", o.t, p->t;
    格式化 "PTR   %d %d %d\\n", (整)(q + 1) - (整)q, (整)大小(q[0]), q[1] == 池[1];
    返 0;
}
'''

EXPECT = {
    'CHAIN': 'CHAIN 20 20 20 20',
    'DEEP': 'DEEP 41 41 41',
    'BFLD': 'BFLD 2 2 2',
    'TFLD': 'TFLD 7 7',
    # PTR: T** 的元素 = 8 字节指针 —— 步长 8 / sizeof(q[0]) 8 / q[1] 命中 池[1]
    'PTR': 'PTR 8 8 1',
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    args = ap.parse_args()
    cc = args.cc if os.path.isabs(args.cc) else os.path.join(ROOT, args.cc)
    if not os.path.exists(cc):
        print('[SKIP] 环境未就绪: %s 不存在' % cc)
        return 2
    os.makedirs(WORK, exist_ok=True)
    p = os.path.join(WORK, 'c1.c')
    with open(p, 'w', encoding='utf-8', newline='\n') as f:
        f.write(SRC)
    exe = os.path.join(WORK, 'c1.exe')
    print('# C1-ELMSIZE 判据  cc=%s' % os.path.basename(cc))
    r = subprocess.run([cc, p, '-o', exe], cwd=ROOT, capture_output=True,
                       env=dict(os.environ, QCC_SMALL_HEAP='1'), timeout=300)
    txt = (r.stdout + r.stderr).decode('utf-8', 'replace')
    if r.returncode != 0:
        print('[编译] rc=%d : FAIL  %s' % (r.returncode, txt.strip().replace('\n', ' | ')[:200]))
        return 1
    e = subprocess.run([exe], capture_output=True, timeout=60)
    out = e.stdout.decode('utf-8', 'replace').strip().splitlines()
    allok = (e.returncode == 0)
    seen = {}
    for line in out:
        toks = line.split()
        if toks:
            seen[toks[0]] = toks          # 按 token 比对（源里的对齐空格不参与）
    for key, exp in EXPECT.items():
        got = seen.get(key, ['<缺失>'])
        good = (got == exp.split())
        allok = allok and good
        print('[%-5s] %r 期望 %r : %s' % (key, ' '.join(got), exp, 'PASS' if good else 'FAIL'))
    print('# 汇总: %s' % ('同一链四种写法取值一致' if allok else '存在不一致'))
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
