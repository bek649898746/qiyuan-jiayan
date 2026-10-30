# -*- coding: utf-8 -*-
"""ORD-52 判据：块内函数原型 `T 名 (形参表);`（C99 6.7.5.3）。

病灶（2026-10-30 修前）：块内声明读完名后词元停在 `(`（型17），落 `语句必收("声明")`
⟹ **A1004「缺少 分号」**。真实载体 = mingw `<stdlib.h>` 的 `strtod` 体内
`double __attribute__((__cdecl__)) __mingw_strtod (const char *, char **);`
（lua `lmathlib.c` / `loslib.c` 的共同首错）。

修法（两刀，均在块内声明的**首声明符读名点**）：
① 读完名若紧跟 `(` ⟹ 按括号配平跳形参表 + `;`，**不登记变量**；
② 原型名登记到 **声明表** `串升一[]` ⟹ 调用点 A1001 门放行（不设 `间拆`）。

判据四格（每格独立编译，记录 rc 与首行诊断）：
  A 最小形态：`int main (void) { int f (int); return 0; }`        —— 修前 A1004
  B 原型 + 调用（真实载体形态）：static 函数体内声明 inner 后调用 —— 修前 A1004 / A1001
  C 对照：无原型的普通块内声明 `int f;` 仍须 rc=0（不得回归）
  D 负控：原型**丢分号** `int f (int) return 0;` 仍须响亮拒绝（rc≠0）

用法: python scripts/check_blockproto.py [--cc qcc_zh_v1.exe]
"""
import argparse
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_blockproto')

CASES = [
    ('A_minimal', 'int main (void) {\n  int f (int);\n  return 0;\n}\n', 1),
    ('B_proto_call',
     'static double wrap (const char *s, char **e) {\n'
     '  double inner (const char *a, char **b);\n'
     '  return inner(s, e);\n'
     '}\n'
     'int main (void) { return 0; }\n', 1),
    ('C_control_plain', 'int main (void) {\n  int f;\n  return 0;\n}\n', 1),
    ('D_neg_missing_semi', 'int main (void) {\n  int f (int)\n  return 0;\n}\n', 0),
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
        first = ((p.stderr or '').strip().splitlines() or [''])[0][:110]
        mark = 'PASS' if ok == bool(want_ok) else 'FAIL'
        if mark == 'FAIL':
            bad += 1
        print('[%s] %-20s rc=%d 期望%s  %s' % (mark, name, p.returncode,
                                              'rc=0' if want_ok else 'rc≠0', first))
    print('==== check_blockproto: %s (%d/%d) ====' % ('PASS' if bad == 0 else 'FAIL',
                                                     len(CASES) - bad, len(CASES)))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
