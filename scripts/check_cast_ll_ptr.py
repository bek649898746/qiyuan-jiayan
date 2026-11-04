# -*- coding: utf-8 -*-
"""check_cast_ll_ptr — `(long long)(指针)` 必须保留完整 64 位。

## 由来

2026-10-31 从 `C1-ELMSIZE` 的 PTR 腿拆出（那条判据的主题是「四种写法取值一致」，
指针宽度是另一件事，混在一起会互相掩盖）。

A/B 事实：
  · 旧编译器 `qcc_zh_v2.zombie.exe`（D4181022）⟹ `PTR 8 8 1` PASS
  · 新编译器 `qcc_zh_v1/v2.exe`（≥95E76CB8）⟹ 差值为指针低 32 位垃圾 ⟹ FAIL

## 判据

`(long long)(q + 1) - (long long)q` 必须等于 8（q 是 `单 **`，步长 = 8 字节）。
不做 `%lld` 打印（运行时无该格式符支持），改用**64 位比较**判定，避免量具本身失真。

用法: python scripts/check_cast_ll_ptr.py [--cc qcc_zh_v1.exe]
退出码：0 = 正确；1 = 截断（缺陷存在）。
"""
import argparse
import io
import os
import subprocess
import sys
import sys as _sys_tmp
_sys_tmp.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402  A1

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = tmp_dir('scratch_cast_ll_ptr')

SRC = '''# 包含 <stdio.h>
函数 整 主(空){
    单 fa = 1.5f, fb = 2.5f;
    单 *池[2]; 池[0] = &fa; 池[1] = &fb;
    单 **q = 池;
    long long 差 = (long long)(q + 1) - (long long)q;
    若 (差 == 8) { 格式化 "OK\\n", 0; } 否 { 格式化 "BAD 低32=%d\\n", (整)差; }
    /* ORD-20261031-67 扩项（gcc 14.2.0 派生）: 无符号 8 字节 转型 / uintptr_t / 嵌套括号 */
    unsigned long long u1 = (unsigned long long)(q + 1) - (unsigned long long)q;
    uintptr_t u2 = (uintptr_t)(q + 1) - (uintptr_t)q;
    整 arr[3]; 整 *p = arr;
    long long n1 = (long long)((p + 1) + 1) - ((long long)(p + 1) + 1);
    long long n2 = (long long)((p + 1) + 1) - (long long)(p + 2);
    long long n3 = (long long)((p + 1) + 1) - (long long)(p + 1);
    /* PTRDIFF 扩项（gcc 14.2.0 派生）: 指针 − 指针 = ptrdiff = **整数**（C11 6.5.6p9），
       不得 再 按 元素 宽 缩放；且 与 操作数 的 左右 形态 无关。修前 实测 x1=5 / x2=-3 / x3=3 / x4=5。 */
    long long x1 = ((p + 1) - p) + 1;   /* gcc 2 */
    long long x2 = ((p + 1) - p) - 1;   /* gcc 0 */
    整 *p2 = arr + 2;
    long long x3 = ((p + 1) - p2) + 1;  /* gcc 0 */
    long long x4 = 1 + ((p + 1) - p);   /* 右操作数 形态: gcc 2 */
    long long x5 = ((p + 1) - p2) - 1;  /* 双指针差 后 **减** 常数（面板 第二轮）: gcc -2 */
    long long x6 = ((p + 2) - p) + 1;   /* 非单位 步数（面板 第二轮）: gcc 3 */
    /* ⚠️ typedef 声明符列表（`, *B`）的用例 已 拆到 **独立判据** `check_typedef_list.py`
       （ORD-20261101-80）—— 混在本判据里会让「修前红」的失败点变成 A1001（未登记别名），
       掩盖 ptrdiff 本身的错值证据。 */
    若 (u1 == 8 && u2 == 8 && n1 == 3 && n2 == 0 && n3 == 4
        && x1 == 2 && x2 == 0 && x3 == 0 && x4 == 2 && x5 == -2 && x6 == 3) { 格式化 "EXT OK\\n", 0; }
    否 { 格式化 "EXT BAD %llu %llu %lld %lld %lld %lld %lld %lld %lld %lld %lld\\n", u1, u2, n1, n2, n3, x1, x2, x3, x4, x5, x6; }
    返 0;
}
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    os.makedirs(WORK, exist_ok=True)
    p = os.path.join(WORK, 'cast_ll_ptr.jy')
    exe = os.path.join(WORK, 'cast_ll_ptr.exe')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(SRC)
    r = subprocess.run([cc, p, '-o', exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=180)
    if r.returncode != 0:
        print('[编译] rc=%d : %s' % (r.returncode, ((r.stdout or '') + (r.stderr or '')).strip()[:200]))
        return 1
    q = subprocess.run([exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=60)
    out = (q.stdout or '').strip()
    print('[run] %s' % out)
    if out.startswith('OK') and 'EXT OK' in out:

        print('[PASS] (long long)(指针) 保留 64 位，步长 = 8')
        return 0
    print('[FAIL] (long long)(指针) 被截断到 32 位（期望 8）')
    return 1


if __name__ == '__main__':
    sys.exit(main())
