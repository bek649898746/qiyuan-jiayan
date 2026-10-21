#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""repro_dtor_chain —— TypeDrop「容器 × 类型图」最小复现（≤100 行）

⚠️ **负结果工具**（2026-10-21 实测，留档以免下次重走）:
   本文件按「每层 2 个嵌入/指针字段 ⟹ 路径 2^N ⟹ 耗时超线性」的假设生成复现源，
   但 **实测在改前编译器上也不超线性**（N=8/16/24/28 → 解析 1/2/5/25ms，见下表），
   即 `类型含析构字段深` 对**合成**结构体图并不展开（`毛利` 只在特定登记路径上设置）。
   ⟹ 热点不可用单标量驱动量复现；真实驱动是「**每个容器变量** × 每张**真实**类型图」
      （lua lparser.c 168 次顶层扫描 / 19.6s ⟹ ≈117ms 每次）。
   本脚本仍保留：`--cc` 时打印三点表，`--show` 时只打印源码（供人工核对 ≤100 行）。

用法:
  python scripts/repro_dtor_chain.py --show
  python scripts/repro_dtor_chain.py --cc qcc_zh_seed.exe --ns 8,16,24,28
"""
import argparse
import os
import subprocess
import sys
import tempfile
import time

TEMPLATE_HEAD = '# 包含 <stdio.h>\n整 计数 = 0;\n构 旁 { 整 z; }\n空 析旁(构 旁 *p) { 计数++; }\n析构 旁, 析旁;\n'


def gen(n, m=2):
    """n 层：C0..C(n-1)，第 i 层嵌入 C(i+1)/C(i+2)；最深一层是标量。"""
    L = [TEMPLATE_HEAD.rstrip('\n')]
    for i in range(n - 1, -1, -1):
        nxt = [j for j in (i + 1, i + 2) if j < n]
        if nxt:
            L.append('构 C%d { %s 整 x; }' % (i, ' '.join('C%d a%d;' % (j, k) for k, j in enumerate(nxt))))
        else:
            L.append('构 C%d { 整 x; }' % i)
    L.append('构 包 { C0 子; }')
    L.append('函数 整 主(空) {')
    for k in range(m):
        L.append('    构 包 v%d; v%d.子.x = %d;' % (k, k, k))
    L.append('    返 0;')
    L.append('}')
    return '\n'.join(L) + '\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='')
    ap.add_argument('--ns', default='8,16,24,28')
    ap.add_argument('--runs', type=int, default=1)
    ap.add_argument('--show', action='store_true')
    a = ap.parse_args()
    ns = [int(x) for x in a.ns.split(',')]
    if a.show or not a.cc:
        src = gen(ns[-1])
        print(src)
        print('# 行数 =', len(src.splitlines()))
        return 0
    tmp = tempfile.mkdtemp(prefix='dtorchain_')
    print('%-4s %8s %10s %8s' % ('N', '字节', '解析ms', '总s'))
    for n in ns:
        src = os.path.join(tmp, 'n%d.c' % n)
        open(src, 'w', encoding='utf-8', newline='\n').write(gen(n))
        env = dict(os.environ, QCC_DBG_TIME='1')
        parse = None
        best = None
        for _ in range(a.runs):
            t = time.perf_counter()
            r = subprocess.run([a.cc, '-c', src, '-o', src + '.o'], capture_output=True, env=env, timeout=1800)
            dt = time.perf_counter() - t
            if r.returncode != 0:
                print('%-4d 编译失败 rc=%d' % (n, r.returncode))
                return 1
            for line in (r.stderr or b'').decode('utf-8', 'replace').splitlines():
                if line.startswith('[TIME]') and '解析=' in line:
                    for seg in line[6:].split():
                        k, _, v = seg.partition('=')
                        if k == '解析' and v.endswith('ms'):
                            parse = int(v[:-2])
            best = (parse, dt)
        print('%-4d %8d %10s %8.2f' % (n, os.path.getsize(src), best[0] if best[0] is not None else '-', best[1]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
