#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_array_dim_decl —— 声明维度里的常量表达式（ORD-20261027-09）

病灶（修前）：局部/结构体声明维度走 `常量表达式求值` 的括号分支，而该分支**不校验 `)`**
  且**不认逗号** ⟹ `(1, 30)` 被求值成 1、词位停在 `,` ⟹ 其后的 `30 ) ]` 无人消费 ⟹
  A1004「声明之后必须分号」（`unsigned int nums[(unsigned int)30 + 1];` / `[(1, 30)]` 两形态）。
修法：括号分支改为「循环吃逗号取末值（C 语义）+ **必须**见到 `)` 才算成功」，失败回退词位。

判据形态：**同一组声明，甲言编出来的程序 vs C 语义期望值，逐项比对**（量 `大小(数组)/大小(元素)`）。
期望值由 `gcc 14.2.0 -O0` 跑镜像 C 得到（`--gcc` 可现场复算）。

用法：
  python scripts/check_array_dim_decl.py --cc qcc_zh_v1.exe
  python scripts/check_array_dim_decl.py --gcc
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

SRC = '''# 包含 <stdio.h>\n常量 KU = (0 ? 1u : -1) >> 30;\n常量 KW = (0 ? 5u : -2) >> 1;\n常量 KS = (0 ? 1 : -1) >> 30;\n常量 KC = (1u ? -1 : 3) >> 31;\n常量 KD = (-4) >> 1u;\n常量 KE = (0u - 1) >> 1;\n常量 KCMP = (-1 < 1u);\n常量 KDIV = (-1) / 2u;\n常量 KMOD = (-1) % 2u;\n常量 KDIV2 = (-1) / 2;\n常量 KMOD2 = (-1) % 2;\n无符号 整 取无(空) { 返 2; }\n函数 整 主(空) {\n    整 a[(1, 30)]; 无符号 整 b[(无符号 整)30 + 1]; 整 c[(1, 2, 3)];\n    整 d[(2, 5) * 2]; 整 e[((1, 4))]; 整 f[(1, 2) + (3, 4)];\n    整 h[(1, 2)][(3, 4)];\n    整 i[(1 ? 30 : 40)]; 整 j[(1 && 1, 5)]; 整 k[(0 || 2, 7)]; 整 l[(0 ? 3 : 4)];\n    整 i2[(0 ? 1u : -1) >> 30];\n    整 a2[((1u ? -1 : 3) >> 31) + 2];\n    整 b2[((-4) >> 1u) + 5];\n    整 i3[((-1 < 1u) ? 3 : 5)];\n    格式化 "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\\n",\n        (整)(大小(a) / 大小(a[0])), (整)(大小(b) / 大小(b[0])), (整)(大小(c) / 大小(c[0])),\n        (整)(大小(d) / 大小(d[0])), (整)(大小(e) / 大小(e[0])), (整)(大小(f) / 大小(f[0])),\n        (整)(大小(h) / 大小(h[0][0])), (整)(大小(i) / 大小(i[0])), (整)(大小(j) / 大小(j[0])),\n        (整)(大小(k) / 大小(k[0])), (整)(大小(l) / 大小(l[0])),\n        (整)(大小(i2) / 大小(i2[0])), KU, KW, KS,\n        (整)(大小(a2) / 大小(a2[0])), (整)(大小(b2) / 大小(b2[0])), KC, KD, KE;\n    格式化 "%d %d %d\\n", (整)KCMP, (整)(大小(i3) / 大小(i3[0])), (整)(-1 < 1u);\n    格式化 "%d %d %d %d\\n", KDIV, KMOD, KDIV2, KMOD2;\n    整 s = -1; 无符号 整 u = 2;\n    整 c3 = -1; c3 /= u;\n    整 d3 = -1; d3 %= u;\n    整 e3 = -1 / 取无();\n    整 f3 = -1 % 取无();\n    格式化 "%d %d %d %d\\n", s / u, s % u, s / 2, s % 2;\n    格式化 "%d %d %d %d\\n", c3, d3, e3, f3;\n    返 0;\n}\n'''

CSRC = '''#include <stdio.h>\nconst int KU = (0 ? 1u : -1) >> 30;\nconst int KW = (0 ? 5u : -2) >> 1;\nconst int KS = (0 ? 1 : -1) >> 30;\nconst int KC = (1u ? -1 : 3) >> 31;\nconst int KD = (-4) >> 1u;\nconst int KE = (0u - 1) >> 1;\nconst int KCMP = (-1 < 1u);\nconst int KDIV = (-1) / 2u;\nconst int KMOD = (-1) % 2u;\nconst int KDIV2 = (-1) / 2;\nconst int KMOD2 = (-1) % 2;\nstatic unsigned qu2(void) { return 2; }\nint main(void){\n  int a[(1,30)]; unsigned int b[(unsigned int)30+1]; int c[(1,2,3)];\n  int d[(2,5)*2]; int e[((1,4))]; int f[(1,2)+(3,4)];\n  int h[(1,2)][(3,4)];\n  int i[(1 ? 30 : 40)]; int j[(1 && 1, 5)]; int k[(0 || 2, 7)]; int l[(0 ? 3 : 4)];\n  int i2[(0 ? 1u : -1) >> 30];\n  int a2[((1u ? -1 : 3) >> 31) + 2];\n  int b2[((-4) >> 1u) + 5];\n  int i3[((-1 < 1u) ? 3 : 5)];\n  printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\\n",\n    (int)(sizeof(a)/sizeof(a[0])), (int)(sizeof(b)/sizeof(b[0])), (int)(sizeof(c)/sizeof(c[0])),\n    (int)(sizeof(d)/sizeof(d[0])), (int)(sizeof(e)/sizeof(e[0])), (int)(sizeof(f)/sizeof(f[0])),\n    (int)(sizeof(h)/sizeof(h[0][0])), (int)(sizeof(i)/sizeof(i[0])), (int)(sizeof(j)/sizeof(j[0])),\n    (int)(sizeof(k)/sizeof(k[0])), (int)(sizeof(l)/sizeof(l[0])),\n    (int)(sizeof(i2)/sizeof(i2[0])), KU, KW, KS,\n    (int)(sizeof(a2)/sizeof(a2[0])), (int)(sizeof(b2)/sizeof(b2[0])), KC, KD, KE);\n  printf("%d %d %d\\n", KCMP, (int)(sizeof(i3)/sizeof(i3[0])), (int)(-1 < 1u));\n  printf("%d %d %d %d\\n", KDIV, KMOD, KDIV2, KMOD2);\n  int s = -1; unsigned u = 2;\n  int c3 = -1; c3 /= u;\n  int d3 = -1; d3 %= u;\n  int e3 = -1 / qu2();\n  int f3 = -1 % qu2();\n  printf("%d %d %d %d\\n", s / u, s % u, s / 2, s % 2);\n  printf("%d %d %d %d\\n", c3, d3, e3, f3);\n  return 0;\n}\n'''

# gcc 14.2.0 -O0 实测（--gcc 可逐字复算）
EXPECTED = [30, 31, 3, 10, 4, 6, 8, 30, 5, 7, 4, 3, 3, 2147483647, -1, 1, 3, -1, -2, 2147483647, 0, 5, 0, 2147483647, 1, 0, -1, 2147483647, 1, 0, -1, 2147483647, 1, 2147483647, 1]


def run(cmd, timeout=300):
    p = subprocess.run(cmd, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=timeout)
    return p.returncode, (p.stdout or '') + (p.stderr or '')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--gcc', action='store_true', help='用 gcc 现场复算期望值并与冻结表核对')
    ap.add_argument('--keep', action='store_true')
    a = ap.parse_args()

    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 找不到编译器: %s' % cc)
        return 2

    tmp = tmp_dir('dimdecl')
    try:
        if a.gcc:
            gcc = shutil.which('gcc')
            if not gcc:
                print('[SKIP] 无 gcc')
                return 2
            csrc = os.path.join(tmp, 'ref.c')
            cexe = os.path.join(tmp, 'ref.exe')
            io.open(csrc, 'w', encoding='utf-8', newline='\n').write(CSRC)
            rc, out = run([gcc, '-O0', '-w', '-o', cexe, csrc])
            if rc != 0:
                print('[FAIL] gcc 编译失败\n' + out[-400:])
                return 1
            rc, out = run([cexe])
            got = [int(x) for x in re.findall(r'-?\d+', out)]
            if got != EXPECTED:
                print('[FAIL] gcc 复算 != 冻结表\n  gcc=%s 冻结=%s' % (got, EXPECTED))
                return 1
            print('[PASS] gcc 复算 = 冻结表（%d 项）' % len(EXPECTED))
            return 0

        src = os.path.join(tmp, 'dim.jy')
        exe = os.path.join(tmp, 'dim.exe')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
        rc, out = run([cc, src, '-o', exe])
        if rc != 0 or not os.path.exists(exe):
            print('[FAIL] 编译失败 rc=%d\n%s' % (rc, out[-600:]))
            return 1
        rc, out = run([exe])
        if rc != 0:
            print('[FAIL] 产物运行失败 rc=%d\n%s' % (rc, out[-300:]))
            return 1
        got = [int(x) for x in re.findall(r'-?\d+', out)]
        if got != EXPECTED:
            print('[FAIL] 声明维度与 C 语义不符\n  期望 %s\n  实得 %s' % (EXPECTED, got))
            return 1
        print('[PASS] 声明维度常量表达式与 C 语义一致（%d 项，编译器=%s）'
              % (len(EXPECTED), os.path.basename(cc)))
        return 0
    finally:
        if not a.keep:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
