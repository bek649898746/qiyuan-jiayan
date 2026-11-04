#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_runtime_div_unsigned —— 运行期 `/` `%` 的混合符号（usual arithmetic conversions）

**当前状态：GREEN（2026-10-31 修复；台账 ORD-20261031-77 已闭环）**。本脚本是它的**常驻判据**。

覆盖三条腿：
  ① 变量除数：`整 s=-1; 无符号 整 u=2; s/u`、`s%u`；
  ② 复合赋值：`c /= u`、`d %= u`（除零包装是**另一条**发射路径，曾经漏修）；
  ③ **用户函数返回无符号作操作数**：`-1 / 取无()`、`-1 % 取无()` **以及比较** `s < 取无()` / `取无() < s`
     —— 比较腿与 `/` `%` 共用同一条 `表达式是否无符号(节点_调用)` 谓词，用来证明修的是**通用谓词**，
     不是给 `__qcc_div_check` 包装节点打特例（若谓词只服务除法，比较腿必红）。

C 语义（C11 6.5.5）：`int / unsigned`、`int % unsigned` 先把 int 转成 unsigned 再运算。
gcc 14.2.0 实测（`整 s=-1; 无符号 整 u=2;`）：`s/u` = 2147483647，`s%u` = 1。
本编译器修前/现状：`s/u` = 0，`s%u` = -1 —— **常量侧已按 C 修好**（`check_array_dim_decl.py`），
运行期代码生成未应用同一转换 ⟹ 同一表达式两种口径。

判据形态：编译 + 运行真实产物，与 gcc 期望逐项比对；同时给出常量侧对照，确保两口径可见。
用法：python scripts/check_runtime_div_unsigned.py --cc qcc_zh_v1.exe
"""
from qcc_tmp import tmp_dir, tmp_root
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

SRC = '''# 包含 <stdio.h>
常量 KS = (-1) / 2u;
常量 KM = (-1) % 2u;
无符号 整 取无(空) { 返 2; }
函数 整 主(空) {
    整 s = -1;
    无符号 整 u = 2;
    整 rs = s / u;
    整 rm = s % u;
    整 ss = s / 2;
    整 sm = s % 2;
    整 c = -1; c /= u;
    整 d = -1; d %= u;
    整 e = -1 / 取无();
    整 f = -1 % 取无();
    格式化 "%d %d %d %d | %d %d
", rs, rm, ss, sm, KS, KM;
    格式化 "%d %d %d %d
", c, d, e, f;
    格式化 "%d %d\n", (s < 取无()), (取无() < s);
    返 0;
}
'''

# 运行期 4 项 + 常量 2 项；gcc 14.2.0 实测
# 运行期 4 + 常量 2 + 复合赋值 2 + 调用 2 + **比较 2**；gcc 14.2.0 实测
EXPECTED = [2147483647, 1, 0, -1, 2147483647, 1, 2147483647, 1, 2147483647, 1, 0, 1]


def run(cmd, timeout=300):
    p = subprocess.run(cmd, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=timeout)
    return p.returncode, (p.stdout or '') + (p.stderr or '')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--keep', action='store_true')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 找不到编译器: %s' % cc)
        return 2
    tmp = tmp_dir('rtdiv')
    try:
        src = os.path.join(tmp, 't.jy')
        exe = os.path.join(tmp, 't.exe')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
        rc, out = run([cc, src, '-o', exe])
        if rc != 0 or not os.path.exists(exe):
            print('[FAIL] 编译失败 rc=%d\n%s' % (rc, out[-400:]))
            return 1
        rc, out = run([exe])
        got = [int(x) for x in re.findall(r'-?\d+', out)]
        if got == EXPECTED:
            print('[PASS] 运行期混合符号 / %% 与 gcc 一致（%d 项，编译器=%s）'
                  % (len(EXPECTED), os.path.basename(cc)))
            return 0
        bad = [(i, EXPECTED[i], got[i]) for i in range(min(len(got), len(EXPECTED)))
               if got[i] != EXPECTED[i]]
        print('[FAIL] 运行期混合符号 / %% 与 C 语义不符（%d 项；台账 ORD-20261031-77）' % len(bad))
        names = ['s/u', 's%u', 's/2', 's%2', '常量 (-1)/2u', '常量 (-1)%2u', 'c/=u', 'd%=u',
                 '-1/取无()', '-1%取无()', 's<取无()', '取无()<s']
        for i, e, g in bad[:8]:
            print('   - %-14s 期望 %d 实得 %d' % (names[i] if i < len(names) else '#%d' % i, e, g))
        print('   实得全量: %s' % (got,))
        return 1
    finally:
        if not a.keep:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
