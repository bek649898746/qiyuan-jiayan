# -*- coding: utf-8 -*-
"""check_array_addr_cast —— **数组名 → 整数转型** 的取址口径判据（ORD-20261101-81）。

## 病灶（2026-11-01 发现，pre-existing：21412505 与 D0D9AF22 同病）

`(长整)数组名` 求值为 **0**（`字` 数组得 1、`构` 数组得 0），而 `(长整)&arr[0]` 正确；
`(长整)(arr + 0)` 也正确 ⟹ **只有「数组变量节点 直接 进 转型」这一形态 丢失取址**（未衰减/未取址）。
后果是静默错值：`(long long)(arr + 1) - (long long)arr` 得 8 亿量级垃圾（gcc 4），
指针数组 `(long long)(t + 1) - (long long)t` 同理（gcc 8）。

## 判据（期望值 = gcc 14.2.0 派生）

四形态 各自 `(long long)名 == (long long)&名[0]` 且 步长正确：
`整[3]` · `字[8]` · `构[2]` · `整 *[3]`

用法: python scripts/check_array_addr_cast.py [--cc qcc_zh_v1.exe]
退出码: 0 = 全对；1 = 有偏差；2 = 环境未就绪
"""
import argparse
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(REPO, 'scratch_array_addr_cast')

SRC = '''# 包含 <stdio.h>
typedef 构 { 整 a; 整 b; } AS;
函数 整 主(空){
    整 arr[3]; arr[0] = 5;
    字 buf[8]; buf[0] = 'x';
    AS sa[2]; sa[0].a = 1;
    整 x = 1, y = 2, z = 3; 整 *t[3]; t[0] = &x; t[1] = &y; t[2] = &z;
    long long d1 = (long long)arr - (long long)&arr[0];            /* 0 */
    long long d2 = (long long)buf - (long long)&buf[0];            /* 0 */
    long long d3 = (long long)sa  - (long long)&sa[0];             /* 0 */
    long long d4 = (long long)t   - (long long)&t[0];              /* 0 */
    long long s1 = (long long)(arr + 1) - (long long)arr;          /* 4 */
    long long s2 = (long long)(buf + 1) - (long long)buf;          /* 1 */
    long long s3 = (long long)(sa + 1) - (long long)sa;            /* 8 */
    long long s4 = (long long)(t + 1) - (long long)t;              /* 8 */
    /* 同族扩项：8 字节无符号 / uintptr_t / 指针转型 —— 三者走同一条「8 字节目标」转型路 */
    unsigned long long u1 = (unsigned long long)arr;               /* == &arr[0] */
    uintptr_t u2 = (uintptr_t)arr;                                 /* == &arr[0] */
    字 *cp = (字*)arr; long long q = (long long)cp;                /* == &arr[0] */
    long long e1 = (long long)u1 - (long long)&arr[0];             /* 0 */
    long long e2 = (long long)u2 - (long long)&arr[0];             /* 0 */
    long long e3 = q - (long long)&arr[0];                         /* 0 */
    若 (d1 == 0 && d2 == 0 && d3 == 0 && d4 == 0 && s1 == 4 && s2 == 1 && s3 == 8 && s4 == 8
        && e1 == 0 && e2 == 0 && e3 == 0) {
        格式化 "AAC OK\\n", 0;
    } 否 {
        格式化 "AAC BAD d=%lld,%lld,%lld,%lld s=%lld,%lld,%lld,%lld e=%lld,%lld,%lld\\n",
        d1, d2, d3, d4, s1, s2, s3, s4, e1, e2, e3;
    }
    返 0;
}
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    os.makedirs(WORK, exist_ok=True)
    p = os.path.join(WORK, 'array_addr_cast.jy')
    exe = os.path.join(WORK, 'array_addr_cast.exe')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(SRC)
    r = subprocess.run([cc, p, '-o', exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=180)
    if r.returncode != 0:
        print('[编译红] rc=%d : %s' % (r.returncode, ((r.stdout or '') + (r.stderr or '')).strip()[:240]))
        return 1
    q = subprocess.run([exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=60)
    out = (q.stdout or '').strip()
    print('[run] %s' % out)
    if out.startswith('AAC OK'):
        print('[PASS] 数组名 → 整数转型 = 首元素地址（四形态 + 步长）')
        return 0
    print('[FAIL] 数组名 → 整数转型 丢失取址（期望 d1..d4 = 0, s1..s4 = 4 1 8 8）')
    return 1


if __name__ == '__main__':
    sys.exit(main())
