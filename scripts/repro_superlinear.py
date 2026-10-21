#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""判据② 受控超线性复现器 —— 「阶梯类型图」上的解析耗时（≤100 行源，3 档驱动量）。

## 为什么是这个形状

`类型含析构字段深(构序)` 的驱动量**不是**单一标量，而是「**容器变量数 × 类型图里的无环路径数**」。
要让**单一**驱动量（层数 k）就呈超线性，必须让「图里路径数」本身随 k 指数增长：

    构 Ck   { 整 v; }                    ← 叶子
    构 Ck-1 { Ck 左; Ck 右; }            ← 两个字段都是 下一层 ⟹ 路径 ×2
    ...
    构 C0   { C1 左; C1 右; }
    析构 独, 独清;                        ← 关键：必须**至少有一条**析构登记，
                                           否则整条分析被全局短路（实测 `深0扫描次=0`）
    函数 整 主(空) { 构 C0 v0 = {0}; 构 C1 v1 = {0}; ... }

从 C0 起有 2^k 条无环路径，且对 C0..Ck-1 各声明一个变量 ⟹ 受控成本 ≈ Σ 2^i ≈ 2^(k+1)。
**层数 k 翻倍（8→16）⟹ 耗时按 2^8 量级增长**，远超判据要求的「≥3×」。

## 判据

| 点 | 驱动量 k | 实测（改前 10-19 版） | 说明 |
|:--|--:|--:|:--|
| 1 | 14 | 588 ms | 基线 |
| 2 | 20 | 649 ms | 路径数 ×64 但还没到「爆点」 |
| 3 | 28 | **12729 ms** | 驱动量 2.00× ⟹ 耗时 **21.7×**（判据要 ≥3×） |

改后（2026-10-21 记忆化+访问集）同一形状仍是 7231 ms（15.4×）⟹ 该形态只治了一半：
访问集只断**当前路径**的环，不合并 DAG 的多路径 ⟹ 已在台账另立一条债（不得当成已修）。

## 用法

  python scripts/repro_superlinear.py --cc qcc_base.exe                 # 只测一个编译器
  python scripts/repro_superlinear.py --cc qcc_base.exe --cc2 qcc_zh_v1.exe
  python scripts/repro_superlinear.py --levels 8,12,16 --runs 3

退出码：0 = 观察到超线性（改前）；1 = 未观察到（判据前提再次被推翻）；2 = 环境缺件。
"""
import argparse
import os
import re
import statistics
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(ROOT, 'scratch_superlinear')


def gen(k):
    """生成 ≤100 行的阶梯源。k = 层数（驱动量）。

    ⚠️ **必须用指针边**（`Ck *左; Ck *右;`）：
      · 嵌入边（`Ck 左;`）会让 `构 C0` 的尺寸 = 4·2^k 字节 —— k=20 就 4MB、k=28 就 1GB，
        编译器在尺寸/栈上先崩，根本跑不到「路径数指数」那一层 ⟹ 实测全持平（旧结论的由来）。
      · 指针边每层只占 8 字节，结构体恒定 16 字节，而 `类型含析构字段深` **不看指针性**
        （只读 `字段.毛利` 这个构序）⟹ 同样是 2^k 条路径，但源只有 34 行。
    """
    L = ['/* 阶梯类型图复现（生成器 scripts/repro_superlinear.py · 勿手改） */',
         '构 独 { 整 x; }',
         '空 独清(构 独 *p) { p->x = 0; }',
         '析构 独, 独清;',
         '构 C%d { 整 v; }' % k]
    for i in range(k - 1, -1, -1):
        L.append('构 C%d { C%d *左; C%d *右; }' % (i, i + 1, i + 1))
    L.append('构 C0 建0(空) { 构 C0 o; o.左 = 0; o.右 = 0; 返 o; }')
    L.append('函数 整 主(空) {')
    L.append('    { 构 C0 a = 建0(); }')
    L += ['    返 0;', '}']
    return '\n'.join(L) + '\n'


def run_once(cc, src, out, env, timeout):
    t0 = time.perf_counter()
    p = subprocess.run([cc, '-c', src, '-o', out], cwd=ROOT, capture_output=True, env=env, timeout=timeout)
    wall = (time.perf_counter() - t0) * 1000.0
    txt = (p.stdout + p.stderr).decode('utf-8', 'replace')
    if p.returncode != 0:
        raise RuntimeError('rc=%d %s' % (p.returncode, txt.strip()[:200]))
    m = re.search(r'解析=(\d+)ms', txt)
    return (int(m.group(1)), wall) if m else (None, wall)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', required=True, help='改前（基线）编译器')
    ap.add_argument('--cc2', default=None, help='改后编译器（对照）')
    ap.add_argument('--levels', default='14,20,28')
    ap.add_argument('--runs', type=int, default=3)
    ap.add_argument('--timeout', type=int, default=900)
    args = ap.parse_args()

    levels = [int(x) for x in args.levels.split(',')]
    os.makedirs(WORK, exist_ok=True)
    env = dict(os.environ, QCC_DBG_TIME='1', QCC_SMALL_HEAP='1')

    ccs = [('before', args.cc)] + ([('after', args.cc2)] if args.cc2 else [])
    for _, exe in ccs:
        p = exe if os.path.isabs(exe) else os.path.join(ROOT, exe)
        if not os.path.exists(p):
            print('[SKIP] 环境未就绪: %s 不存在' % p)
            return 2

    table = []
    for tag, exe in ccs:
        exe = exe if os.path.isabs(exe) else os.path.join(ROOT, exe)
        for k in levels:
            src = os.path.join(WORK, 'sl%d.c' % k)
            with open(src, 'w', encoding='utf-8', newline='\n') as f:
                f.write(gen(k))
            lines = gen(k).count('\n')
            parses, walls = [], []
            for i in range(args.runs):
                parse_ms, wall_ms = run_once(exe, src, os.path.join(WORK, 'sl%d_%d.o' % (k, i)),
                                             env, args.timeout)
                parses.append(parse_ms if parse_ms is not None else -1)
                walls.append(round(wall_ms, 1))
            use = parses if all(x >= 0 for x in parses) else None
            val = statistics.median(use if use else walls)
            table.append({'who': tag, 'k': k, 'lines': lines, 'median_ms': val,
                          'mode': 'parse' if use else 'wall', 'runs': use if use else walls})
            print('[RUN] %-6s k=%-2d  源 %2d 行  中位 %8.1f ms (%-5s)  逐次=%s'
                  % (tag, k, lines, val, 'parse' if use else 'wall', use if use else walls), flush=True)

    ok = True
    for tag in ('before', 'after'):
        rows = [r for r in table if r['who'] == tag]
        if len(rows) < 2:
            continue
        base = rows[0]
        last = rows[-1]
        ratio = last['median_ms'] / base['median_ms'] if base['median_ms'] else 0
        driver_x = last['k'] / base['k']
        print('[JUDGE] %-6s 驱动量 k=%d→%d（%.2f×）⟹ 耗时 %.1f→%.1f ms（%.2f×）  %s'
              % (tag, base['k'], last['k'], driver_x, base['median_ms'], last['median_ms'], ratio,
                 '超线性 OK' if ratio >= 3.0 else '未超线性'))
        if tag == 'before':
            ok = ratio >= 3.0
    print('[JUDGE] 结论: %s' % ('改前编译器上复现出超线性（判据② 可满足）' if ok
                                else '未复现超线性（判据② 前提再次被推翻，需裁决）'))
    print('[FILE] 生成源在 %s（最大 %d 行）' % (WORK, max(r['lines'] for r in table)))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
