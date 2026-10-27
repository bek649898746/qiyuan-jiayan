# -*- coding: utf-8 -*-
"""check_prelude_macros —— 内建 prelude 标准宏的永久闸（施工单《端到端三项缺口》P0）。

判据：下面 18 个宏**不带任何 -D** 就能用，且值正确（源内是 #包含 + 真运行断言）。
覆盖 <stdio.h> EOF/BUFSIZ/FILENAME_MAX/SEEK_* · <stdlib.h> EXIT_*/RAND_MAX ·
<math.h> HUGE_VAL/INFINITY/NAN/M_PI/M_E · <errno.h> EINVAL/ENOMEM/ENOENT ·
<time.h> CLOCKS_PER_SEC。

用法: python scripts/check_prelude_macros.py [--cc exe]
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SRC = r'''#包含 <stdio.h>
#包含 <stdlib.h>
#包含 <math.h>
#包含 <errno.h>
#包含 <time.h>

静 整 判(整 条, 整 好) { 若 (!好) { 格式化打印("FAIL %d\n", 条); 返 1; } 返 0; }

整 主(空) {
    若 (判(1, EOF == -1)) 返 1;
    若 (判(2, BUFSIZ == 8192)) 返 1;
    若 (判(3, FILENAME_MAX >= 256)) 返 1;
    若 (判(4, SEEK_SET == 0 && SEEK_CUR == 1 && SEEK_END == 2)) 返 1;
    若 (判(5, EXIT_SUCCESS == 0 && EXIT_FAILURE == 1)) 返 1;
    若 (判(6, RAND_MAX == 32767)) 返 1;
    若 (判(7, HUGE_VAL > 1.0e300)) 返 1;
    若 (判(8, INFINITY > 1.0e300)) 返 1;
    若 (判(9, NAN != NAN)) 返 1;
    若 (判(10, M_PI > 3.14159 && M_PI < 3.14160)) 返 1;
    若 (判(11, M_E > 2.71828 && M_E < 2.71829)) 返 1;
    若 (判(12, ENOENT == 2 && ENOMEM == 12 && EINVAL == 22)) 返 1;
    若 (判(13, CLOCKS_PER_SEC == 1000)) 返 1;
    格式化打印("PASS\n");
    返 0;
}
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or
                    (os.path.join(REPO, 'qcc_zh_seed.exe') if os.path.exists(os.path.join(REPO, 'qcc_zh_seed.exe'))
                     else os.path.join(REPO, 'qcc_zh_v1.exe')))
    a = ap.parse_args()
    d = tempfile.mkdtemp(prefix='qcc_prelude_')
    src = os.path.join(d, 'p.c')
    exe = os.path.join(d, 'p.exe')
    open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
    r = subprocess.run([a.cc, src, '-o', exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if r.returncode != 0:
        print('[FAIL] prelude 宏探针 编译失败:')
        print(((r.stdout or '') + (r.stderr or '')).strip()[-400:])
        return 1
    p = subprocess.run([exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    out = ((p.stdout or '') + (p.stderr or '')).strip()
    if p.returncode != 0 or 'PASS' not in out:
        print('[FAIL] prelude 宏探针 运行失败 rc=%d: %s' % (p.returncode, out[-300:]))
        return 1
    print('[PASS] prelude 标准宏 13 组（EOF/BUFSIZ/FILENAME_MAX/SEEK_*/EXIT_*/RAND_MAX/HUGE_VAL/INFINITY/NAN/M_PI/M_E/E*/CLOCKS_PER_SEC）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
