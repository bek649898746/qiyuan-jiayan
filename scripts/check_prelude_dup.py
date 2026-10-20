# -*- coding: utf-8 -*-
"""check_prelude_dup — 审计 P0-1 判据（2026-10-21 虾米）

病灶：`compat_prelude.h` 里 `compress`/`compress2`/`uncompress` **各定义两遍**（逐字相同）⟹
任何 `-I compat_prelude.h` 的编译都 `A6109 'compress2' 重复定义`（git 语料编译链 `_git_compat_compile.py`
正是全程带 `-I compat_prelude.h`）⟹ 「能编译 git」当前不成立。

判据（两条都过才算 PASS）：
 ① `qcc min.c -I compat_prelude.h` ⟹ **rc=0**（修前 rc=1 + A6109）
 ② `qcc min.c`（不带 -I）⟹ rc=0（对照：不因修复而破坏主路）

用法: python scripts/check_prelude_dup.py [--cc <exe>]
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
SRC = '整 主(空) { 返 0; }\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')
    a = ap.parse_args()
    d = tempfile.mkdtemp(prefix='predup_')
    src = os.path.join(d, 'min.c')
    open(src, 'w', encoding='utf-8-sig').write(SRC)
    ok = True

    r = subprocess.run([a.cc, src, '-I', os.path.join(REPO, 'compat_prelude.h'),
                        '-o', os.path.join(d, 'a.exe')], capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    out = (r.stdout or '') + (r.stderr or '')
    good = (r.returncode == 0)
    print('① 带 -I compat_prelude.h: rc=%d %s -> %s'
          % (r.returncode, out.strip()[-100:], 'PASS' if good else 'FAIL'))
    ok &= good

    r2 = subprocess.run([a.cc, src, '-o', os.path.join(d, 'b.exe')], capture_output=True, text=True,
                        encoding='utf-8', errors='replace')
    good2 = (r2.returncode == 0)
    print('② 不带 -I（对照）: rc=%d -> %s' % (r2.returncode, 'PASS' if good2 else 'FAIL'))
    ok &= good2

    print('=== AUDIT-P0-1 判据（prelude 无重复定义）: %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
