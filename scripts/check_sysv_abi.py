# -*- coding: utf-8 -*-
"""SysV ABI 互调判据（甲言 .o ↔ gcc .o）—— fail-closed。

三组（每组都必须**实跑**并核对数值，不是只看链接成功）：
  A 甲言 提供函数 / gcc 提供 main
  B gcc 提供函数 / 甲言 提供 main
  C 格式化：甲言 .o 里用 snprintf("%d|%s|%llx") 输出，与 gcc 同源实现逐字节比对

用法: python scripts/check_sysv_abi.py [--cc qcc_zh_v1.exe] [--keep]
退出: 0 全过；1 任一红（缺 WSL/gcc 亦 FAIL）
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
from qcc_tmp import tmp_dir

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK_BASE = os.environ.get('QCC_ELF_WORK', r'D:\qcc_tmp')

# 甲言侧：提供 add / dadd（gcc 侧调用）
JY_LIB = r'''
int add(int a, int b) { return a + b; }
double dadd(double a, double b) { return a + b; }
long long ladd(long long a, long long b) { return a + b; }
'''

# gcc 侧：调用甲言函数
GCC_MAIN = r'''
#include <stdio.h>
int add(int, int);
double dadd(double, double);
long long ladd(long long, long long);
int main(void) {
    int i = add(3, 4);
    double d = dadd(1.5, 2.25);
    long long l = ladd(10000000000LL, 23LL);
    printf("i=%d d=%.2f l=%lld\n", i, d, l);
    return (i == 7 && l == 10000000023LL) ? 0 : 1;
}
'''

# 甲言侧：main 调 gcc 提供的函数
JY_MAIN_CALLS_GCC = r'''
#include <stdio.h>
int gcc_mul(int a, int b);
int gcc_main_probe(void);
int main(void) { printf("m=%d\n", gcc_mul(6, 7)); return gcc_mul(6, 7) == 42 ? 0 : 1; }
'''

GCC_LIB = r'''
int gcc_mul(int a, int b) { return a * b; }
'''

# 格式化：甲言 .o 里调 snprintf（libc），main 由 gcc 提供
JY_FMT = r'''
int fmt(char *buf, int n) {
    return snprintf(buf, n, "%d|%s|%llx", 42, "hi", (unsigned long long)0xdeadbeefcafeULL);
}
/* ORD-20261105-SYSV: C 组补 %x / %f / 宽度 / 精度 —— 浮点（xmm 通道 + %al）与整型
   宽度在原格式串里都未覆盖；同一 .o 内再导出一个函数，由 gcc 侧逐字节比对。 */
int fmtf(char *buf, int n) {
    return snprintf(buf, n, "%x|%f|%8.3f|%.2e|%5.1f|%5d|%.3s",
                    255, 3.5, 3.5, 1234.5, 2.25, 42, "abcdef");
}
'''

GCC_FMT_MAIN = r'''
#include <stdio.h>
int fmt(char *, int);
int fmtf(char *, int);
int main(void) {
    char a[128]; char b[128];
    char c[128]; char d[128];
    int ra, rb, rc, rd;
    ra = fmt(a, sizeof a);
    rb = snprintf(b, sizeof b, "%d|%s|%llx", 42, "hi", (unsigned long long)0xdeadbeefcafeULL);
    rc = fmtf(c, sizeof c);
    rd = snprintf(d, sizeof d, "%x|%f|%8.3f|%.2e|%5.1f|%5d|%.3s",
                  255, 3.5, 3.5, 1234.5, 2.25, 42, "abcdef");
    printf("jy=%s\ngcc=%s\n", a, b);
    printf("jyf=%s\ngccf=%s\n", c, d);
    printf("r=%d,%d,%d,%d\n", ra, rc, rb, rd);
    if (ra != rb || rc != rd) return 2;
    if (__builtin_strcmp(a, b) != 0) return 1;
    if (__builtin_strcmp(c, d) != 0) return 3;
    return 0;
}
'''


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8',
                          errors='replace', **kw)


def wsl_bash(script):
    return run(['wsl', '-d', 'Ubuntu', '--', 'bash', '-c', script])


def wp(win_path):
    p = os.path.abspath(win_path).replace('\\', '/')
    return '/mnt/' + p[0].lower() + p[2:] if len(p) > 1 and p[1] == ':' else p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--keep', action='store_true')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)

    if shutil.which('wsl') is None:
        print('[FAIL] 缺 wsl.exe'); return 1
    if 'Linux' not in (wsl_bash('uname -s').stdout or ''):
        print('[FAIL] WSL 不可用'); return 1
    if wsl_bash('which gcc').returncode != 0:
        print('[FAIL] WSL 缺 gcc'); return 1

    os.makedirs(WORK_BASE, exist_ok=True)
    w = tmp_dir('sysvchk')
    env = dict(os.environ, QCC_SMALL_HEAP='1')
    fails = []

    def jy(name, src, obj):
        p = os.path.join(w, name)
        with open(p, 'w', encoding='utf-8', newline='\n') as f:
            f.write(src)
        r = run([cc, '-c', '--target=elf', p, '-o', os.path.join(w, obj)], cwd=ROOT, env=env)
        if r.returncode != 0:
            fails.append('%s 编译失败: %s' % (name, ((r.stdout or '') + (r.stderr or '')).strip()[:200]))
            return False
        return True

    def sh(name, body):
        p = os.path.join(w, name)
        with open(p, 'w', encoding='utf-8', newline='\n') as f:
            f.write('#!/bin/bash\ncd %s\n%s\n' % (wp(w), body))
        return wsl_bash("bash '%s'" % wp(p))

    # ---------- A: 甲言 .o + gcc main ----------
    if jy('jylib.c', JY_LIB, 'jylib.o'):
        with open(os.path.join(w, 'gmain.c'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(GCC_MAIN)
        r = sh('a.sh', 'gcc gmain.c jylib.o -o a.out 2>&1 && ./a.out; echo "__RC=$?"')
        out = r.stdout or ''
        print('[A] ' + out.strip().replace('\n', ' | ')[:200])
        if '__RC=0' not in out:
            fails.append('A 互调失败: %s' % out.strip()[:200])
        elif 'i=7 d=3.75 l=10000000023' not in out:
            fails.append('A 数值不符: %s' % out.strip()[:200])

    # ---------- B: 甲言 main + gcc .o ----------
    if jy('jymain.c', JY_MAIN_CALLS_GCC, 'jymain.o'):
        with open(os.path.join(w, 'gclib.c'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(GCC_LIB)
        r = sh('b.sh', 'gcc -c gclib.c -o gclib.o && gcc jymain.o gclib.o -o b.out 2>&1 && ./b.out; echo "__RC=$?"')
        out = r.stdout or ''
        print('[B] ' + out.strip().replace('\n', ' | ')[:200])
        if '__RC=0' not in out or 'm=42' not in out:
            fails.append('B 互调失败: %s' % out.strip()[:200])

    # ---------- C: 格式化（libc snprintf via SysV varargs） ----------
    if jy('jyfmt.c', JY_FMT, 'jyfmt.o'):
        with open(os.path.join(w, 'gfmt.c'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(GCC_FMT_MAIN)
        r = sh('c.sh', 'gcc gfmt.c jyfmt.o -o c.out 2>&1 && ./c.out; echo "__RC=$?"')
        out = r.stdout or ''
        print('[C] ' + out.strip().replace('\n', ' | ')[:320])
        if '__RC=0' not in out:
            fails.append('C 格式化不一致: %s' % out.strip()[:320])
        elif 'jyf=' not in out or 'gccf=' not in out:
            fails.append('C 浮点/宽度/精度组未执行: %s' % out.strip()[:320])

    if fails:
        print('\n[FAIL] ' + '\n       '.join(fails))
        if not a.keep:
            shutil.rmtree(w, ignore_errors=True)
        return 1
    print('\n[PASS] SysV 互调 A/B + 格式化 C 全过')
    if a.keep:
        print('  保留: %s' % w)
    else:
        shutil.rmtree(w, ignore_errors=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
