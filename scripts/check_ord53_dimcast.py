# -*- coding: utf-8 -*-
"""ORD-53 判据：**常量上下文 的 转型操作数** `(T)x`（数组维度 / 枚举值 / case 标签）。

病灶（2026-10-30 修前）：`常量表达式求值` 的 `末列()` 把 `(` **无条件** 当括号表达式，
`(int)4` 里的型词 `int` 不是操作数 ⟹ 求值 返 0，但 `(` 已被消耗且 **词位不回退**
⟹ 数组维度 求值 从 `int` 继续 ⟹ 声明收口 全错位：
  `static void f (void) { unsigned int nums[(int)4]; int i; }`
  ⟹ `A1004 缺少 分号 ';' ... 当前 词元 = int[型2], 前四: nums | | |`
真实载体 = lua `ltable.c` / `onelua.c` 的 `unsigned int nums[MAXABITS + 1];`
（`#define MAXABITS cast_int(sizeof(int) * CHAR_BIT - 1)` ⟹ `((int)(...)) + 1`）。

判据五格：
  A 最小形态 `[(int)4]`                            —— 修前 A1004
  B 真实载体形态 `[(int)(sizeof(int)*8-1) + 1]`     —— 修前 A1004
  C 对照 `[(4)]`（括号非转型）仍须 rc=0（不得回归）
  D 对照 `[4]`（字面量）仍须 rc=0
  E 负控 `[(int]`（转型缺右括）仍须 rc≠0（不得静默放过）

用法: python scripts/check_ord53_dimcast.py [--cc qcc_zh_v1.exe]
"""
import argparse
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord53')

BODY = ('static void f (void) {\n'
        '  unsigned int nums[%s];\n'
        '  int i;\n'
        '  nums[0] = 0;\n'
        '  for (i = 0; i < 3; i++) nums[i] = i;\n'
        '}\n'
        'int main (void) { f(); return 0; }\n')

CASES = [
    ('A_cast', BODY % '(int)4', 1),
    ('B_cast_sizeof', BODY % '(int)(sizeof(int) * 8 - 1) + 1', 1),
    ('C_paren', BODY % '(4)', 1),
    ('D_literal', BODY % '4', 1),
    ('E_neg_unclosed', BODY % '(int', 0),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[SKIP] 编译器不存在: %s' % cc)
        return 0
    os.makedirs(OUT, exist_ok=True)
    bad = 0
    for name, src, want_ok in CASES:
        c = os.path.join(OUT, name + '.c')
        o = os.path.join(OUT, name + '.o')
        with open(c, 'w', encoding='utf-8', newline='\n') as f:
            f.write(src)
        p = subprocess.run([cc, '-c', c, '-o', o], cwd=REPO, capture_output=True,
                           text=True, encoding='utf-8', errors='replace')
        ok = (p.returncode == 0)
        first = ((p.stderr or '').strip().splitlines() or [''])[0][:100]
        mark = 'PASS' if ok == bool(want_ok) else 'FAIL'
        if mark == 'FAIL':
            bad += 1
        print('[%s] %-16s rc=%d 期望%s  %s' % (mark, name, p.returncode,
                                              'rc=0' if want_ok else 'rc≠0', first))
    print('==== check_ord53_dimcast: %s (%d/%d) ====' % ('PASS' if bad == 0 else 'FAIL',
                                                         len(CASES) - bad, len(CASES)))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
