#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""C3-A4001 判据 —— 「结构体指针字段 / 结构体数组字段 / 写内层字段」组合下的
A4001「变量未初始化」误报检查。

## 背景

台账 C3 记的是「三者同时在场 ⟹ A4001 组合误报」。2026-10-21 按描述构造的单形态已不复现，
但**原现场未最小化** ⟹ 本判据把 3 个在场条件的组合逐一真编译 + 真运行：

| 例 | 形态 | 期望 |
|:--|:--|:--|
| `B`     | 只有数组字段：`o.arr[0].x = 7; o.arr[1].x = 8;` 读回 | `C3 79` |
| `A`     | 只有指针字段：`内 v; o.p = &v; o.p->x = 5;` 读回 | `C3 6` |
| `AB`    | 指针 + 数组，**两者都写内层** | `C3 84`（5 + 7×10 + 8 + 1）|
| `ABC`   | 指针 + 数组 + 下标链（同 AB 形态）| `C3 84` |

任一组报 A4001（编译失败）或输出不符 ⟹ FAIL。

用法：
    python scripts/check_c3_a4001.py              # 默认 qcc_zh_v1.exe
    python scripts/check_c3_a4001.py --cc qcc_pd.exe
退出码：0 = 全符合；1 = 有红；2 = 环境缺件。
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(ROOT, 'scratch_c3')

CASES = {
    # tag: (字段声明, 函数体语句, 期望输出)
    'B': ('构 内 arr[2]; 整 t;',
          ['构 外 o;',
           'o.arr[0].x = 7; o.arr[1].x = 8;',
           'o.t = 1;',
           '格式化 "C3 %d\\n", o.arr[0].x * 10 + o.arr[1].x + o.t;'],
          'C3 79'),
    'A': ('构 内 *p; 整 t;',
          ['构 内 v; v.x = 0;',
           '构 外 o;',
           'o.p = &v; o.p->x = 5;',
           'o.t = 1;',
           '格式化 "C3 %d\\n", o.p->x + o.t;'],
          'C3 6'),
    'AB': ('构 内 *p; 构 内 arr[2]; 整 t;',
           ['构 内 v; v.x = 0;',
            '构 外 o;',
            'o.p = &v; o.p->x = 5;',
            'o.arr[0].x = 7; o.arr[1].x = 8;',
            'o.t = 1;',
            '格式化 "C3 %d\\n", o.p->x + o.arr[0].x * 10 + o.arr[1].x + o.t;'],
           'C3 84'),
    'ABC': ('构 内 *p; 构 内 arr[2]; 整 t;',
            ['构 内 v; v.x = 0;',
             '构 外 o;',
             'o.p = &v; o.p->x = 5;',
             'o.arr[0].x = 7; o.arr[1].x = 8;',
             'o.t = 1;',
             '格式化 "C3 %d\\n", o.p->x + o.arr[0].x * 10 + o.arr[1].x + o.t;'],
            'C3 84'),
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
    print('# C3-A4001 判据  cc=%s' % os.path.basename(cc))
    allok = True
    for tag, (fields, body, exp) in CASES.items():
        L = ['# 包含 <stdio.h>', '构 内 { 整 x; }', '构 外 { %s }' % fields, '函数 整 主(空) {']
        L += ['    ' + b for b in body]
        L += ['    返 0;', '}']
        src = '\n'.join(L) + '\n'
        p = os.path.join(WORK, tag + '.c')
        with open(p, 'w', encoding='utf-8', newline='\n') as f:
            f.write(src)
        exe = os.path.join(WORK, tag + '.exe')
        r = subprocess.run([cc, p, '-o', exe], cwd=ROOT, capture_output=True,
                           env=dict(os.environ, QCC_SMALL_HEAP='1'), timeout=300)
        txt = (r.stdout + r.stderr).decode('utf-8', 'replace')
        if r.returncode != 0:
            print('[%s] 编译失败 rc=%d : FAIL  %s' % (tag, r.returncode, txt.strip().replace('\n', ' | ')[:150]))
            allok = False
            continue
        e = subprocess.run([exe], capture_output=True, timeout=60)
        out = e.stdout.decode('utf-8', 'replace').strip()
        good = (e.returncode == 0 and out == exp)
        allok = allok and good
        print('[%-3s] rc=%d out=%r 期望=%r : %s' % (tag, e.returncode, out, exp, 'PASS' if good else 'FAIL'))
    print('# 汇总: %s' % ('全部符合（无 A4001 误报、值全对）' if allok else '有红'))
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
