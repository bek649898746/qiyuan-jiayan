#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_stderr575 —— R575 判据：四种写入函数的 stderr 分派。

修前（R575 现场）：编译器把 `stderr`/`stdout`/`stdin` 三个全局都折叠成 0，
`fprintf` 内建只看「函数指针 == 0 → STD_OUTPUT_HANDLE」⟹ 两路全进 stdout。
第一版修复只覆盖 `fprintf`；`fputs`/`fwrite`/`fputc` 会把 0xFFFF 当 Win32 句柄
传给 WriteFile ⟹ 静默丢输出（skeptic 2026-10-20 抓出）。本判据同时钉四种函数。

判据（真实入口，不 mock）：
  ① 用被测编译器编 `probe.c`，rc=0；
  ② 运行后单独捕获 stdout/stderr；
  ③ 每个标记必须出现在正确流、且不得混入另一流。
用法: python scripts/check_stderr575.py [--cc qcc_zh_v1.exe]
退出码: 0 = PASS；1 = RED
"""
import argparse
import os
import subprocess
import sys
import tempfile
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

PROBE = '''#包含 <stdio.h>
函数 整 主(空) {
    fprintf(stderr, "FPRINTF-ERR\\n");
    fprintf(stdout, "FPRINTF-OUT\\n");
    fputs("FPUTS-ERR\\n", stderr);
    fputs("FPUTS-OUT\\n", stdout);
    fwrite("FWRITE-ERR\\n", 1, 11, stderr);
    fwrite("FWRITE-OUT\\n", 1, 11, stdout);
    fputc('Z', stderr);
    fputc('Y', stdout);
    返 0;
}
'''

ERR_MARKS = ['FPRINTF-ERR', 'FPUTS-ERR', 'FWRITE-ERR', 'Z']
OUT_MARKS = ['FPRINTF-OUT', 'FPUTS-OUT', 'FWRITE-OUT', 'Y']


def run(cmd, timeout=120):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout,
                          encoding='utf-8', errors='replace')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo)
    tmp = tempfile.mkdtemp(prefix='stderr575_')
    src = os.path.join(tmp, 'probe.c')
    exe = os.path.join(tmp, 'probe.exe')
    with open(src, 'w', encoding='utf-8', newline='\n') as f:
        f.write(PROBE)

    c = run([a.cc, src, '-o', exe])
    print('[①] compile rc=%d %s' % (c.returncode, (c.stdout + c.stderr).strip()[:160]))
    if c.returncode != 0:
        print('=== stderr575: RED (compile) ===')
        return 1

    r = run([exe], timeout=30)
    out, err = r.stdout, r.stderr
    print('[②] run rc=%d' % r.returncode)
    print('[③] stdout=%r' % out)
    print('[③] stderr=%r' % err)

    ok = True
    for m in ERR_MARKS:
        if m not in err:
            print('  - stderr 缺 %s' % m)
            ok = False
        if m in out:
            print('  - stdout 混入 %s（错流）' % m)
            ok = False
    for m in OUT_MARKS:
        if m not in out:
            print('  - stdout 缺 %s' % m)
            ok = False
        if m in err:
            print('  - stderr 混入 %s（错流）' % m)
            ok = False
    if r.returncode != 0:
        print('  - 运行 rc != 0')
        ok = False

    print()
    if ok:
        print('=== stderr575: PASS ===')
        return 0
    print('=== stderr575: RED ===')
    return 1


if __name__ == '__main__':
    sys.exit(main())
