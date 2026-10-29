# -*- coding: utf-8 -*-
"""ORD-34 复现：`-S` 编**编译器主源**崩溃（0xC0000005）。blocked（需专门一轮定位）。

判据 = 用 --cc 指定的编译器跑 `-S srclib_jiayan/qcc_zh.jy`。
  rc 为 0xC0000005 / >= 0xC0000000 ⟹ 崩溃复现（rc=1，blocked 诚实）
  rc == 0 且 .asm 非空 ⟹ 崩溃消失 ⟹ 应转 fixed
用法: python scripts/check_ord34_scrash.py [--cc qcc_zh_v1.exe]
"""
import argparse, os, subprocess, sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord34')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    ap.add_argument('--timeout', type=int, default=900)
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    os.makedirs(TMP, exist_ok=True)
    out = os.path.join(TMP, 'main_S.exe')
    asm = out + '.asm'
    for p in (out, asm):
        if os.path.exists(p):
            os.remove(p)
    try:
        p = subprocess.run([cc, '-S', 'srclib_jiayan/qcc_zh.jy', '-o', out], cwd=REPO,
                           capture_output=True, text=True, encoding='utf-8', errors='replace',
                           timeout=a.timeout)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        print('[FAIL] -S 超时（%ds）' % a.timeout)
        return 2
    size = os.path.getsize(asm) if os.path.exists(asm) else 0
    if rc == 0 and size > 0:
        print('[PASS] -S 编主源不再崩溃（asm=%d 字节）⟹ ORD-34 应转 fixed' % size)
        return 0
    print('[REPRO] -S 编主源崩溃复现: rc=%d (0x%08X) asm=%d 字节 ⟹ blocked 诚实' % (rc, rc & 0xFFFFFFFF, size))
    return 1


if __name__ == '__main__':
    sys.exit(main())
