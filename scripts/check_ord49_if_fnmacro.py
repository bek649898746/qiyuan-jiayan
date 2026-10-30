# -*- coding: utf-8 -*-
"""ORD-49 判据：`#if` 表达式里的**函数式宏**必须先展开再求值（C11 6.10.1）。

修前（不动点 2DEBC46E 及以前）：求值器只认 数字/对象宏/defined()，函数宏名落到原子分支被当 0
  ⟹ `#if FIGS <= 32`（FIGS = l_floatatt(MANT_DIG) = 53）被算成 `0 <= 32` 真
  ⟹ **静默把假分支当真分支编译**（gcc 选 #else；实测 lua lmathlib.c 因此编不过）。

判据 = 编一个含两分支标记的小样本，看 `.asm` 里出现哪个分支：
  · 出现 branch_false 且不出现 branch_true ⟹ PASS（与 gcc 一致）
  · 出现 branch_true                      ⟹ FAIL（静默选错分支，修前状态）
另外用 gcc 做**同源 oracle**：gcc -E 也必须只剩 branch_false（gcc 缺失时跳过该腿并注明）。

用法: python scripts/check_ord49_if_fnmacro.py [--cc qcc_zh_v1.exe]
"""
import argparse
import os
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord49')

SRC = '''#define l_floatatt(n)\t(DBL_##n)
#define DBL_MANT_DIG\t53
#define MANT_DIG\t53
#define FIGS\t\tl_floatatt(MANT_DIG)

#if FIGS <= 32
int branch_true (void) { return 1; }
#else
int branch_false (void) { return 2; }
#endif

#if 53 <= 32
int lit_true (void) { return 1; }
#else
int lit_false (void) { return 2; }
#endif

int main (void) { return 0; }
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
    src = os.path.join(TMP, 'ifx.c')
    out = os.path.join(TMP, 'ifx.exe')
    asm = out + '.asm'
    for p in (out, asm):
        if os.path.exists(p):
            os.remove(p)
    with open(src, 'w', encoding='utf-8', newline='\n') as f:
        f.write(SRC)
    r = subprocess.run([cc, '-S', src, '-o', out], capture_output=True, timeout=300)
    text = open(asm, encoding='utf-8', errors='replace').read() if os.path.exists(asm) else ''
    if r.returncode != 0 or not text:
        print('[FAIL] 编译失败 rc=%d %s' % (r.returncode,
              (r.stderr or b'').decode('utf-8', 'replace').strip()[:160]))
        return 1
    got_true = 'branch_true' in text
    got_false = 'branch_false' in text
    got_lit_true = 'lit_true' in text
    got_lit_false = 'lit_false' in text
    print('[qcc] branch_true=%s branch_false=%s | 字面量腿: lit_true=%s lit_false=%s'
          % (got_true, got_false, got_lit_true, got_lit_false))

    gcc = shutil.which('gcc') or r'C:\mingw-w64\bin\gcc.exe'
    if os.path.exists(gcc):
        g = subprocess.run([gcc, '-E', src], capture_output=True)
        gtxt = (g.stdout or b'').decode('utf-8', 'replace')
        print('[gcc oracle] branch_true=%s branch_false=%s'
              % ('branch_true' in gtxt, 'branch_false' in gtxt))
        if ('branch_true' in gtxt) != got_true or ('branch_false' in gtxt) != got_false:
            print('[FAIL] 与 gcc 分支选择不一致 ⟹ 仍在静默选错分支')
            return 1
    else:
        print('[gcc oracle] 缺失，跳过对照腿')

    if got_lit_true or not got_lit_false:
        print('[FAIL] 字面量对照腿不成立（`#if 53 <= 32` 应假）⟹ 判据自检失败')
        return 1
    if got_true or not got_false:
        print('[FAIL] 函数宏腿 选错分支 ⟹ 缺陷仍在')
        return 1
    print('[PASS] `#if` 函数式宏 已按 C11 6.10.1 先展开再求值（与 gcc 一致）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
