# -*- coding: utf-8 -*-
"""check_fn_param_cap —— 大函数数源码的容量护栏判据（`fn_param` 与 补丁表）。

背景（2026-10-28 · R893）：
  · `fn_param` 原是**固定 4096 条**（按函数一条），4000 个三参数函数即触发
    `A5001 参数映射表 fn_param 满（容量 4096）`。已改为**无固定上限的动态表**（首块 4096、满时翻倍）。
  · 8,000 函数源继续撞**下一张表**：`[ERR] 补丁表 溢出 (容量 0)`（全局 `全败容` 初值 1048576，
    实测报 0 ⟹ 疑似某次重置把容量清了），对应台账 `PATCH-TABLE-CAP`（blocked）。

判据：
  · `--n 4000`（默认）：8,000 行级源码必须 rc=0（`FN-PARAM-DYN` 的修后绿）；
  · `--n 8000`：8,000 函数必须 rc=0（`PATCH-TABLE-CAP` 的复现␣，当前 rc=1）。

用法: python scripts/check_fn_param_cap.py [--cc qcc_zh_v1.exe] [--n 4000]
退出码: 0 = 该规模编译成功；1 = 触发护栏/失败；2 = 环境未就绪
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def gen(n, path):
    L = ['#include <stdio.h>', 'int main(void){ return 0; }']
    for i in range(n):
        L.append('int f%d(int a%d, int b%d, int c%d){ return a%d + b%d + c%d + %d; }'
                 % (i, i, i, i, i, i, i, i))
    io.open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')
    return len(L)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_v1.exe'))
    ap.add_argument('--n', type=int, default=4000)
    # 修前腿用（与 run_criteria --product 同语义，命令末尾追加 ⟹ 仍是同源前缀）
    ap.add_argument('--product', default=None, help='修前腿：用这个旧编译器产物代替 --cc')
    a = ap.parse_args()
    a.cc = a.product or a.cc
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到编译器 %s（不判绿）' % cc)
        return 2

    w = tempfile.mkdtemp(prefix='qcc_fnparam_')
    src = os.path.join(w, 'many.c')
    lines = gen(a.n, src)
    out = os.path.join(w, 'many.exe')
    r = subprocess.run([cc, src, '-o', out], cwd=ROOT, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=1800)
    msg = ((r.stdout or '') + (r.stderr or '')).strip()
    last = msg.splitlines()[-1][:150] if msg else ''
    print('n=%-5d lines=%-6d rc=%d  %s' % (a.n, lines, r.returncode, last))
    if r.returncode != 0:
        print('[FAIL] %d 个函数（%d 行）编译失败' % (a.n, lines))
        return 1
    print('[PASS] %d 个函数（%d 行）编译成功' % (a.n, lines))
    return 0


if __name__ == '__main__':
    sys.exit(main())
