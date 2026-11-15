# -*- coding: utf-8 -*-
"""check_printf_bloat.py —— 产物过大·printf 调用点膨胀判据（ORD-20261110-PRINTF）.

根因（2026-11-10 实测）：`printf` 是编译器内建，`缺对()` 每次调用都把整段格式解释器 `度壬`
内联一遍 ⟹ **≈7.1 KB/调用点**。本轮改为「每函数每内建种类只发一份共享块，其余站点 `call`」。

四条：
  A. 每点增量：N=1 与 N=20（同函数、同格式）的编译器自报 code 字节差 ÷ 19 **≤ 200 B**；
  B. 负控：同一源加 `--no-printf-share` ⟹ 每点增量 **≥ 1000 B**（证明增益来自共享，不是别处）；
  C. 行为：N=20 程序真跑，输出 20 行且首尾逐字正确；
  D. 对照：普通函数调用（同函数、N=1/20）每点增量同样 ≤200 B（量具本身不自带膨胀）。

用法: python scripts/check_printf_bloat.py [--cc qcc_zh_v1.exe]
退出码: 0 = PASS / 1 = FAIL / 2 = 环境未就绪
"""
import argparse
import io
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)


def gen_printf(n):
    L = ['#include <stdio.h>', 'int main(void){']
    L += ['  printf("v%d=%%d\\n", %d);' % (i, i) for i in range(n)]
    L += ['  return 0;', '}']
    return '\n'.join(L) + '\n'


def gen_call(n):
    L = ['#include <stdio.h>', 'static int f(int x){ return x + 1; }', 'int main(void){ int s = 0;']
    L += ['  s = s + f(%d);' % i for i in range(n)]
    L += ['  printf("%d\\n", s);', '  return 0;', '}']
    return '\n'.join(L) + '\n'


def code_bytes(cc, src_text, tag, extra=()):
    src = os.path.join(WORK, tag + '.c')
    exe = os.path.join(WORK, tag + '.exe')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(src_text)
    r = subprocess.run([cc, src, '-o', exe] + list(extra), capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if r.returncode != 0:
        print('[ERR] %s 编译失败 rc=%d\n%s' % (tag, r.returncode, (r.stdout + r.stderr)[-500:]))
        return None, None
    m = re.search(r'\((\d+) bytes code', r.stdout)
    return (int(m.group(1)) if m else None), exe


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    global WORK
    WORK = tmp_dir('printf_bloat')
    if not os.path.exists(a.cc):
        print('环境未就绪：缺 %s' % a.cc)
        return 2

    bad = []
    s1, e1 = code_bytes(a.cc, gen_printf(1), 'p1')
    s20, e20 = code_bytes(a.cc, gen_printf(20), 'p20')
    if s1 is None or s20 is None:
        print('[失败 FAIL] printf 探针 编译失败')
        return 1
    per = (s20 - s1) / 19.0
    print('A printf 每点增量: N=1 %d → N=20 %d，Δ=%.1f B/点（上限 200）' % (s1, s20, per))
    if per > 200:
        bad.append('A printf 每点 %.1f B > 200' % per)

    b1, _ = code_bytes(a.cc, gen_printf(1), 'n1', ['--no-printf-share'])
    b20, _ = code_bytes(a.cc, gen_printf(20), 'n20', ['--no-printf-share'])
    if b1 is None or b20 is None:
        bad.append('B 负控 编译失败（--no-printf-share 不可用）')
    else:
        bper = (b20 - b1) / 19.0
        print('B 负控（--no-printf-share）每点: N=1 %d → N=20 %d，Δ=%.1f B/点（应 ≥1000）' % (b1, b20, bper))
        if bper < 1000:
            bad.append('B 负控每点 %.1f B < 1000（增益来源可疑）' % bper)

    # C 行为
    r = subprocess.run([e20], capture_output=True)
    lines = r.stdout.splitlines()
    ok_c = (r.returncode == 0 and len(lines) == 20 and lines[0] == b'v0=0' and lines[-1] == b'v19=19')
    print('C 行为: rc=%d lines=%d first=%r last=%r' % (r.returncode, len(lines), lines[:1], lines[-1:] if lines else []))
    if not ok_c:
        bad.append('C N=20 运行结果不对')

    c1, _ = code_bytes(a.cc, gen_call(1), 'c1')
    c20, _ = code_bytes(a.cc, gen_call(20), 'c20')
    if c1 is not None and c20 is not None:
        cper = (c20 - c1) / 19.0
        print('D 对照（普通函数调用）每点: Δ=%.1f B/点' % cper)
        if cper > 200:
            bad.append('D 普通调用每点 %.1f B > 200（量具自带膨胀）' % cper)

    print()
    if bad:
        print('[失败 FAIL] check_printf_bloat：')
        for b in bad:
            print('  - ' + b)
        return 1
    print('[通过 PASS] check_printf_bloat：printf 每点 %.1f B（≤200）· 负控 %.1f B（≥1000）· 行为一致' % (per, bper))
    return 0


if __name__ == '__main__':
    sys.exit(main())
