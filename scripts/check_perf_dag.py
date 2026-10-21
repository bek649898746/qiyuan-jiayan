#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""PERF-DAG 判据 —— TypeDrop 的类型图遍历必须（a）只看**内嵌**字段、（b）在环形图上终止、
（c）**不**把「指向析构类型的指针字段」当成容器自己拥有的析构字段。

## 三例各自对应一条真缺陷（同一根因：字段记录 `毛利` 一身二职）

`毛利` 对「指向结构体的指针字段」存的是**被指结构体的构序**（`a.p->x` 的类型推断要用），
而 `类型含析构字段深()` / `合成发射()` 把它当「**内嵌**结构体构序」来递归 ⟹ 指针边也进遍历：

| 例 | 症状 | 现实后果 |
|:--|:--|:--|
| `ladder` | 指针边把无环图的路径数变成 2^k ⟹ 解析耗时指数 | 10 行源即可让编译慢到不可用 |
| `cycle`  | 指针边构成环 ⟹ `合成发射()` 无环守卫 ⟹ **编译器挂死**（不返回） | 用户代码一写互指结构体就卡死 |
| `ptr`    | 只含「指向析构类型的指针」的容器被判「含析构字段」 | ①强制声明即初始化（A3110）②退出时对**不归它所有**的被指对象调析构 ⟹ 双析构 |

## 用法

    python scripts/check_perf_dag.py                     # 默认 qcc_zh_v1.exe，跑三例
    python scripts/check_perf_dag.py --cc qcc_old.exe    # 在旧编译器上取 RED
    python scripts/check_perf_dag.py --only ladder

退出码：0 = 三例全绿；1 = 有红；2 = 环境缺件。
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SELF = os.path.join(ROOT, 'scratch_perf_dag')

# ladder: 指针边阶梯（结构体恒定 16B）；k=16 与 k=28 的路径数差 4096 倍
LADDER_LO, LADDER_HI = 16, 28
LADDER_ABS_MS = 150          # k=28 的解析耗时绝对上限（正常应 <10ms）
LADDER_RATIO = 4             # 相对 k=16 的倍数上限

CYCLE_SRC = '''# 包含 <stdio.h>
整 计 = 0;
构 底 { 整 x; }
空 底清(构 底 *p) { 计++; p->x = 0; }
析构 底, 底清;
构 甲 { 构 乙 *pb; 构 底 *pd; }
构 乙 { 构 甲 *pa; }
构 甲 建甲(空) { 构 甲 a; a.pb = 0; a.pd = 0; 返 a; }
函数 整 主(空) {
    { 构 甲 a = 建甲(); }
    格式化 "计=%d\\n", 计;
    返 (计 == 1) ? 0 : 1;
}
'''

PTR_SRC = '''# 包含 <stdio.h>
整 计 = 0;
构 内 { 整 x; }
空 内清(构 内 *p) { 计++; p->x = 0; }
析构 内, 内清;
构 内 建内(空) { 构 内 o; o.x = 0; 返 o; }
构 外 { 构 内 *p; 整 t; }
函数 整 主(空) {
    { 构 内 内值 = 建内(); }
    { 构 外 o = {0}; }
    格式化 "计=%d\\n", 计;
    返 (计 == 1) ? 0 : 1;
}
'''


def gen_ladder(k):
    L = ['构 独 { 整 x; }', '空 独清(构 独 *p) { p->x = 0; }', '析构 独, 独清;', '构 C%d { 整 v; }' % k]
    for i in range(k - 1, -1, -1):
        L.append('构 C%d { C%d *左; C%d *右; }' % (i, i + 1, i + 1))
    L.append('构 C0 建0(空) { 构 C0 o; o.左 = 0; o.右 = 0; 返 o; }')
    L += ['函数 整 主(空) { { 构 C0 a = 建0(); } 返 0; }']
    return '\n'.join(L) + '\n'


def run_cc(cc, src_path, out_path, env, timeout):
    return subprocess.run([cc, '-c', src_path, '-o', out_path], cwd=ROOT,
                          capture_output=True, env=env, timeout=timeout)


def parse_ms(txt):
    m = re.search(r'解析=(\d+)ms', txt)
    return int(m.group(1)) if m else None


def case_ladder(cc, work, env, ok):
    res = {}
    for k in (LADDER_LO, LADDER_HI):
        p = os.path.join(work, 'lad%d.c' % k)
        with open(p, 'w', encoding='utf-8', newline='\n') as f:
            f.write(gen_ladder(k))
        r = run_cc(cc, p, os.path.join(work, 'lad%d.o' % k), dict(env, QCC_DBG_TIME='1'), 900)
        txt = (r.stdout + r.stderr).decode('utf-8', 'replace')
        if r.returncode != 0:
            print('[ladder] k=%d rc=%d %s' % (k, r.returncode, txt.strip()[:120]))
            return False
        ms = parse_ms(txt)
        res[k] = ms
        print('[ladder] k=%-2d 路径数 2^%d  解析=%sms' % (k, k, ms))
    lo, hi = res.get(LADDER_LO), res.get(LADDER_HI)
    if lo is None or hi is None:
        print('[ladder] 未取到 [TIME] 解析= 读数（编译器缺四阶段计时？）')
        return False
    limit = max(LADDER_ABS_MS, LADDER_RATIO * lo)
    good = hi <= limit
    print('[ladder] 判定: 解析(k=%d)=%dms <= max(%d, %d×%d=%d) : %s'
          % (LADDER_HI, hi, LADDER_ABS_MS, LADDER_RATIO, lo, LADDER_RATIO * lo, 'PASS' if good else 'FAIL'))
    return good


def case_cycle(cc, work, env, ok):
    p = os.path.join(work, 'cycle.c')
    with open(p, 'w', encoding='utf-8', newline='\n') as f:
        f.write(CYCLE_SRC)
    try:
        r = run_cc(cc, p, os.path.join(work, 'cycle.o'), env, 20)
    except subprocess.TimeoutExpired:
        print('[cycle] 判定: 编译器 >20s 未返回 ⟹ 挂死 : FAIL')
        return False
    txt = (r.stdout + r.stderr).decode('utf-8', 'replace')
    if r.returncode != 0:
        print('[cycle] 判定: rc=%d ⟹ 编译失败 : FAIL  %s' % (r.returncode, txt.strip()[:140]))
        return False
    print('[cycle] 判定: rc=0 且 20s 内返回 : PASS')
    return True


def case_ptr(cc, work, env, ok):
    p = os.path.join(work, 'ptr.c')
    with open(p, 'w', encoding='utf-8', newline='\n') as f:
        f.write(PTR_SRC)
    try:
        r = run_cc(cc, p, os.path.join(work, 'ptr.o'), env, 60)
    except subprocess.TimeoutExpired:
        print('[ptr] 判定: 编译 >60s : FAIL')
        return False
    txt = (r.stdout + r.stderr).decode('utf-8', 'replace')
    if r.returncode != 0:
        print('[ptr] 判定: 指针字段使容器被判「含析构」⟹ rc=%d : FAIL  %s'
              % (r.returncode, txt.strip()[:140]))
        return False
    print('[ptr] 判定: rc=0（指针字段不要求初始化/不牵连析构） : PASS')
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--only', default='ladder,cycle,ptr')
    args = ap.parse_args()
    cc = args.cc if os.path.isabs(args.cc) else os.path.join(ROOT, args.cc)
    if not os.path.exists(cc):
        print('[SKIP] 环境未就绪: %s 不存在' % cc)
        return 2
    os.makedirs(SELF, exist_ok=True)
    env = dict(os.environ, QCC_SMALL_HEAP='1')
    only = [x.strip() for x in args.only.split(',') if x.strip()]
    print('# PERF-DAG 判据  cc=%s' % os.path.basename(cc))
    results = {}
    for name, fn in (('ladder', case_ladder), ('cycle', case_cycle), ('ptr', case_ptr)):
        if name not in only:
            continue
        results[name] = fn(cc, SELF, env, True)
    allok = all(results.values()) if results else False
    print('# 汇总: ' + ' '.join('%s=%s' % (k, 'PASS' if v else 'FAIL') for k, v in results.items()))
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
