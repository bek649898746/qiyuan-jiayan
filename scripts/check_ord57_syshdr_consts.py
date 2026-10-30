# -*- coding: utf-8 -*-
"""ORD-57 判据：**内置头 / 内建名 缺口**（`<stdio.h>` 三常量 + `<locale.h>` 六常量 + `setlocale`）。

病灶（2026-10-30 修前）：`甲言 跳过 <...> 系统头` ⟹ 其宏/声明不可见 ⟹
  · `char buff[L_tmpnam]`      ⟹ `A1001 未声明标识符 'L_tmpnam'`（lua `loslib.c` 的 `os_tmpname`）
  · `LC_ALL` 等 6 个            ⟹ `A1001 未声明标识符 'LC_ALL'`（`os_setlocale`）
  · `setlocale(...)`           ⟹ `A1001 未声明函数 'setlocale'`
  · `FOPEN_MAX` / `TMP_MAX`    同样缺失（C11 7.21.1 要求）

修法（两处，均为「补内置头 / 补内建名」）：
  ① 内置头宏区补 `L_tmpnam=260` / `FOPEN_MAX=20` / `TMP_MAX=2147483647` +
     `LC_ALL=0` / `LC_COLLATE=1` / `LC_CTYPE=2` / `LC_MONETARY=3` / `LC_NUMERIC=4` / `LC_TIME=5`
     （**值全部取本机 mingw-w64 实测**，不猜）；
  ② 内建名表 `主薄` 补 `setlocale`（与 printf/malloc 同族豁免）。

判据五格（每格真跑 `-c`）：
  A `<stdio.h>` 三常量作数组维度/表达式 ⟹ 期望 rc=0（修前 A1001）
  B `<locale.h>` 六常量求值            ⟹ 期望 rc=0（修前 A1001）
  C `setlocale` 调用                   ⟹ 期望 rc=0（修前 A1001）
  D 对照：普通代码不受影响             ⟹ 期望 rc=0
  E 负控：**未定义** 的宏仍须 A1001     ⟹ 期望 rc≠0（不得因白名单放水）

用法: python scripts/check_ord57_syshdr_consts.py [--cc qcc_zh_v1.exe]
"""
import argparse
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord57')

CASES = [
    ('A_stdio_consts',
     '#include <stdio.h>\n'
     'int main (void) {\n'
     '  char buff[L_tmpnam];\n'
     '  int m = TMP_MAX > 0 && FOPEN_MAX > 0;\n'
     '  buff[0] = 0;\n'
     '  return (int)buff[0] + m;\n'
     '}\n', 1),
    ('B_locale_consts',
     '#include <locale.h>\n'
     'int main (void) {\n'
     '  int v = LC_ALL + LC_COLLATE + LC_CTYPE + LC_MONETARY + LC_NUMERIC + LC_TIME;\n'
     '  return v == 15 ? 0 : 1;\n'
     '}\n', 1),
    ('C_setlocale',
     '#include <locale.h>\n'
     'int main (void) { setlocale(LC_ALL, "C"); return 0; }\n', 1),
    ('D_control',
     'int main (void) { int x = 1; return x - 1; }\n', 1),
    ('E_neg_undefined',
     '#include <stdio.h>\n'
     'int main (void) { char b[ZZZ_NOT_A_REAL_LIMIT]; return 0; }\n', 0),
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
        mark = 'PASS' if ok == bool(want_ok) else 'FAIL'
        if mark == 'FAIL':
            bad += 1
        first = ((p.stderr or '').strip().splitlines() or [''])[0][:90]
        print('[%s] %-18s rc=%d 期望%s  %s' % (mark, name, p.returncode,
                                              'rc=0' if want_ok else 'rc≠0', first))
    print('==== check_ord57_syshdr_consts: %s (%d/%d) ====' % ('PASS' if bad == 0 else 'FAIL',
                                                              len(CASES) - bad, len(CASES)))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
