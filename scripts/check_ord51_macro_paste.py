# -*- coding: utf-8 -*-
"""ORD-51 判据：`##` 拼接必须使用**原样实参**（C11 6.10.3.3）—— `l_floatatt(MANT)` 应得 `DBL_MANT`(=53)，不是 `DBL_53`。

修前（不动点 DEF8508B 及以前）：qcc 报 `A1001 未声明标识符 'DBL_53'`；gcc rc=0。
修后：qcc rc=0，且与 gcc 同源 oracle 一致（都取 53）。

用法: python scripts/check_ord51_macro_paste.py [--cc qcc_zh_v1.exe]
"""
import argparse
import os
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord51')

SRC = '''#define MANT 53
#define DBL_MANT 53
#define l_floatatt(n) (DBL_##n)
#define FIGS l_floatatt(MANT)
static int f (void) { return (int)(FIGS); }
int main (void) { return (f() == 53) ? 0 : 1; }
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: %s' % cc)
        return 2
    os.makedirs(TMP, exist_ok=True)
    src = os.path.join(TMP, 'paste.c')
    exe = os.path.join(TMP, 'paste.exe')
    with open(src, 'w', encoding='utf-8', newline='\n') as f:
        f.write(SRC)
    r = subprocess.run([cc, src, '-o', exe], capture_output=True, timeout=300)
    err = (r.stderr or b'').decode('utf-8', 'replace').strip().replace('\n', ' ')
    print('[qcc] rc=%d %s' % (r.returncode, err[:140]))
    if r.returncode != 0:
        print('[REPRO] `##` 仍用已展开实参（编译不过）⟹ 缺陷仍在')
        return 1
    run = subprocess.run([exe], capture_output=True)
    print('[qcc 运行] rc=%d（0 = 取到 53，与 C11 一致）' % run.returncode)
    if run.returncode != 0:
        print('[FAIL] 编译通过但运行值不是 53 ⟹ `##` 语义仍错')
        return 1
    gcc = shutil.which('gcc') or r'C:\mingw-w64\bin\gcc.exe'
    if os.path.exists(gcc):
        g = subprocess.run([gcc, src, '-o', exe + '.gcc.exe'], capture_output=True)
        gr = subprocess.run([exe + '.gcc.exe'], capture_output=True) if g.returncode == 0 else None
        print('[gcc oracle] 编译 rc=%d 运行 rc=%s' % (g.returncode, gr.returncode if gr else 'n/a'))
        if g.returncode != 0 or (gr and gr.returncode != 0):
            print('[FAIL] gcc 对照腿不成立（样本或环境问题）')
            return 2
    print('[PASS] `##` 拼接用原样实参（C11 6.10.3.3），与 gcc 一致')
    return 0


if __name__ == '__main__':
    sys.exit(main())
