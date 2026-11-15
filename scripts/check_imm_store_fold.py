#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_imm_store_fold —— P2 判据：`移动 rX,K` + `存帧32 [rbp+d], rX`（rX 死）
⟹ `存帧立32 [rbp+d], K`。

三腿（缺一不可）：
  ① 形态：优化产物里 `存帧立32` 出现次数 >= 1（pass 真触发，不是死代码）；
  ② 语义：未优化链 vs 优化链 运行输出 **逐字节相同**、退出码相同；
  ③ 期望：输出必须等于固定串（防「两边一起错」的假绿）。

用法: python scripts/check_imm_store_fold.py [--cc qcc_zh_v2.exe] [--work <dir>]
退出码: 0 = 通过；1 = 判红；2 = 环境未就绪
"""
import argparse
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
try:
    from qcc_tmp import tmp_dir
except Exception:
    tmp_dir = lambda n: os.path.join('build', n)

SRC = r'''
#include <stdio.h>
/* 8 个局部立即数初始化：强制产生「移动 rX,K + 存帧32」对 */
static int 桶(int x) {
    int a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7, h = 8;
    int s = 0;
    s = s + a + b + c + d + e + f + g + h;
    return s + x;
}
/* 死槽：局部只写不读（合成后寄存器值也不再被消费） */
static int 死槽(int x) {
    int a = 11, b = 12, c = 13, d = 14;
    return x * 2;
}
/* 活值密集：合成不得改变任何值 */
static int 活(int x) {
    int a = 3, b = 4, c = 5, 丁 = 6;
    int t = a * b + c - 丁;
    return t + x;
}
int main(void) {
    printf("IMMFOLD %d %d %d %d\n", 桶(0), 死槽(5), 活(0), 活(100));
    return 0;
}
'''
EXPECT = b'IMMFOLD 36 10 11 111\n'


def run(cmd, t=600):
    return subprocess.run(cmd, capture_output=True, timeout=t)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    ap.add_argument('--opt', default='opt_zh.exe')
    ap.add_argument('--work', default=None)
    a = ap.parse_args()
    for p in (a.cc, a.opt, 'asm_zh.exe'):
        if not os.path.exists(p):
            print('环境未就绪：缺 %s' % p)
            return 2

    W = a.work or tmp_dir('immfold')
    os.makedirs(W, exist_ok=True)
    src = os.path.join(W, 'immfold.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)

    base = os.path.join(W, 'base.exe')
    r = run([a.cc, '-S', src, '-o', base])
    if r.returncode != 0:
        print('FAIL: -S 失败 rc=%d\n%s' % (r.returncode, (r.stdout + r.stderr).decode('utf-8', 'replace')[-600:]))
        return 1
    asm = base + '.asm'
    if not os.path.exists(asm):
        print('FAIL: 缺 %s' % asm)
        return 1

    r = run(['asm_zh.exe', asm, '-o', os.path.join(W, 'base_a.exe')])
    if r.returncode != 0:
        print('FAIL: 未优化链 asm 失败')
        return 1
    oasm = os.path.join(W, 'opt.asm')
    r = run([a.opt, asm, '-o', oasm])
    if r.returncode != 0:
        print('FAIL: opt_zh 失败 rc=%d\n%s' % (r.returncode, (r.stdout + r.stderr).decode('utf-8', 'replace')[-600:]))
        return 1
    r = run(['asm_zh.exe', oasm, '-o', os.path.join(W, 'opt_a.exe')])
    if r.returncode != 0:
        print('FAIL: 优化链 asm 失败\n%s' % (r.stdout + r.stderr).decode('utf-8', 'replace')[-600:])
        return 1

    p1 = run([os.path.join(W, 'base_a.exe')], 300)
    p2 = run([os.path.join(W, 'opt_a.exe')], 300)
    text = io.open(oasm, encoding='utf-8', errors='replace').read()
    nfold = text.count('存帧立32')

    print('形态: 存帧立32 = %d' % nfold)
    print('未优化: rc=%d out=%r' % (p1.returncode, p1.stdout))
    print('优化后: rc=%d out=%r' % (p2.returncode, p2.stdout))

    fails = []
    if nfold < 1:
        fails.append('pass 未触发（存帧立32 = 0）')
    if p1.stdout != p2.stdout or p1.returncode != p2.returncode:
        fails.append('两链输出/退出码不等')
    if p2.stdout != EXPECT:
        fails.append('优化链输出 != 期望 %r' % EXPECT)
    if fails:
        print('\n[失败 FAIL] ' + '；'.join(fails))
        return 1
    print('\n[通过 PASS] 存帧立32=%d，两链输出逐字节相同且等于期望' % nfold)
    return 0


if __name__ == '__main__':
    sys.exit(main())
