#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_operator_prec —— 运算符优先级「与 C 对齐」判据（ORD-20261027-25 · bug1/bug2/bug3）

判据形态：**同一组表达式，甲言编出来的程序 vs C 语义期望值，逐项比对**。
期望值不是手算的 —— 由 `gcc 14.2.0 -O0` 跑同一组表达式的镜像 C 程序得到（`--gcc` 可现场复算）。

覆盖三层：
  · bug1 算术/移位：`p >> 2 + 3` 应为 `p >> (2+3)`（2026-10-27 R885 已修，本判据防回归）
  · bug2 位运算 vs 比较：`a*c < b^a` 应为 `((a*c) < b) ^ a`（2026-10-28 R888+ 已修，防回归）
  · bug3 关系 vs 相等：`a == b < c` 应为 `a == (b<c)`（2026-10-31 本刀修，此前按 `(a==b) < c` 静默算错）

用法：
  python scripts/check_operator_prec.py --cc qcc_zh_v1.exe
  python scripts/check_operator_prec.py --gcc        # 用 gcc 复算期望值并核对冻结表
"""
import argparse
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 甲言源（驱动真实的 qcc 前端 + 码生成 + 运行期）
SRC = '''# 包含 <stdio.h>
函数 整 主(空) {
    整 a = 10; 整 b = 4; 整 c = 9; 整 d = 13; 整 p = 16; 整 q = 1; 整 r = 64;
    整 a2 = 2; 整 b2 = 0; 整 c2 = 1;
    格式化 "%d %d %d %d\\n", p >> 2 + 3, q << 2 + 3, r >> 6 - 4, p >> 1 * 2;
    格式化 "%d %d %d %d %d %d %d %d\\n", a * c < b ^ a, b | d >> (b < c & 7), a & 3 + 4, 5 >= a > 7 & c, c ^ d & c * c, c <= d <= d > 9, a | b ^ c & d, (a | b) ^ c & d;
    格式化 "%d %d %d %d %d\\n", a2 == b2 < c2, a2 != b2 < c2, a2 == b2 <= c2, (a2 + 1) == b2 + 1 < c2, a2 == b2 < c2 == 1;
    格式化 "%d %d %d %d %d\\n", b + 1 ^ a & c, d - 2 | b & 3, a * 2 > b ^ 3, r / 8 == p >> 2, (a - 6) == (b - 2) < 1;
    返 0;
}
'''

# 镜像 C（同一组表达式，只用于 --gcc 复算期望值）
CSRC = '''#include <stdio.h>
int main(void){
  int a=10,b=4,c=9,d=13,p=16,q=1,r=64;
  int a2=2,b2=0,c2=1;
  printf("%d %d %d %d\\n", p>>2+3, q<<2+3, r>>6-4, p>>1*2);
  printf("%d %d %d %d %d %d %d %d\\n", a*c < b^a, b | d >> (b < c & 7), a&3+4, 5>=a>7&c, c^d&c*c, c<=d<=d>9, a|b^c&d, (a|b)^c&d);
  printf("%d %d %d %d %d\\n", a2==b2<c2, a2!=b2<c2, a2==b2<=c2, (a2+1)==b2+1<c2, a2==b2<c2==1);
  printf("%d %d %d %d %d\\n", b+1^a&c, d-2|b&3, a*2>b^3, r/8==p>>2, (a-6)==(b-2)<1);
  return 0;
}
'''

# gcc 14.2.0 -O0 的实测输出（--gcc 可逐字复算）
EXPECTED = [
    0, 32, 16, 4,
    10, 6, 2, 0, 8, 0, 15, 7,
    0, 1, 0, 0, 0,
    13, 11, 2, 0, 0,
]


def run(cmd, cwd=None, timeout=300):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=timeout)
    return p.returncode, (p.stdout or '') + (p.stderr or '')


def gcc_expected(tmp):
    gcc = shutil.which('gcc')
    if not gcc:
        print('[SKIP] 无 gcc，无法现场复算期望值')
        return None
    src = os.path.join(tmp, 'prec_ref.c')
    exe = os.path.join(tmp, 'prec_ref.exe')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(CSRC)
    rc, out = run([gcc, '-O0', '-o', exe, src])
    if rc != 0:
        print('[FAIL] gcc 编译镜像 C 失败\n' + out[-500:])
        return None
    rc, out = run([exe])
    if rc != 0:
        print('[FAIL] gcc 产物运行失败\n' + out[-500:])
        return None
    return [int(x) for x in re.findall(r'-?\d+', out)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe', help='被测编译器（默认 qcc_zh_v1.exe）')
    ap.add_argument('--gcc', action='store_true', help='用 gcc 现场复算期望值并与冻结表核对')
    ap.add_argument('--keep', action='store_true', help='保留临时目录（排障用）')
    a = ap.parse_args()

    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 找不到编译器: %s' % cc)
        return 2

    tmp = tempfile.mkdtemp(prefix='prec_')
    try:
        if a.gcc:
            exp = gcc_expected(tmp)
            if exp is None:
                return 2
            if exp != EXPECTED:
                print('[FAIL] gcc 复算结果与冻结表不符\n  gcc  = %s\n  冻结 = %s' % (exp, EXPECTED))
                return 1
            print('[PASS] gcc 复算 = 冻结表（%d 项）' % len(EXPECTED))
            return 0

        src = os.path.join(tmp, 'prec.jy')
        exe = os.path.join(tmp, 'prec.exe')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
        rc, out = run([cc, src, '-o', exe])
        if rc != 0 or not os.path.exists(exe):
            print('[FAIL] 编译失败 rc=%d\n%s' % (rc, out[-800:]))
            return 1
        rc, out = run([exe])
        if rc != 0:
            print('[FAIL] 产物运行失败 rc=%d\n%s' % (rc, out[-400:]))
            return 1
        got = [int(x) for x in re.findall(r'-?\d+', out)]
        if got != EXPECTED:
            bad = [(i, EXPECTED[i], got[i]) for i in range(min(len(got), len(EXPECTED)))
                   if got[i] != EXPECTED[i]]
            print('[FAIL] 优先级与 C 语义不符（%d/%d 项）' % (len(bad), len(EXPECTED)))
            for i, e, g in bad[:12]:
                print('   #%d 期望 %d 实得 %d' % (i + 1, e, g))
            print('   实得全量: %s' % (got,))
            return 1
        print('[PASS] 运算符优先级与 C 语义一致（bug1/bug2/bug3 共 %d 项，编译器=%s）'
              % (len(EXPECTED), os.path.basename(cc)))
        return 0
    finally:
        if not a.keep:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
