# -*- coding: utf-8 -*-
"""check_longlong_ptr —— 局部 `long long *` 的解引用/下标宽度（ORD-20261030-66 复现器）。

实测（qcc_zh_v1/v2）：`long long x = 5000000000LL; long long *p = &x; printf("%lld %lld", *p, p[0])`
输出 `705032704 0`（= 低 32 位），gcc 与 ref_7C276ACB 均 `5000000000 5000000000`。

定位（本轮）：`变量元素()` 取的是 `指元素尺寸`（声明解析直接写），不是 `指针元尺寸()` 写的 `指内`；
`指针元素` 对 `long long` 仍算 4 ⟹ `p[i]` 步长 4、`*p` 读 4 字节。
判据：与 gcc 逐值一致；不一致 ⟹ 复现（rc=1）。
"""
import argparse
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_longlongptr')

SRC = """#include <stdio.h>
int main (void) {
  long long x = 5000000000LL;
  long long *p = &x;
  long long y = *p;
  long long arr[2]; arr[0] = 11; arr[1] = 22;
  long long *q = arr;
  long long z = q[1];
  printf("%lld %lld\\n", y, z);
  return 0;
}
"""


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8',
                          errors='replace', **kw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    os.makedirs(TMP, exist_ok=True)
    src = os.path.join(TMP, 'a.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
    exe = os.path.join(TMP, 'a.exe')
    r = run([cc, src, '-o', exe], cwd=REPO)
    if r.returncode != 0:
        print('[FAIL] 编译 rc=%d %s' % (r.returncode, (r.stderr or '')[:120]))
        return 2
    got = (run([exe]).stdout or '').strip()
    want = '5000000000 22'
    for cand in ('gcc', r'C:\msys64\ucrt64\bin\gcc.exe', r'C:\mingw64\bin\gcc.exe'):
        if run([cand, '--version']).returncode == 0:
            g = os.path.join(TMP, 'g.exe')
            if run([cand, '-O0', '-w', src, '-o', g]).returncode == 0:
                want = (run([g]).stdout or '').strip()
            break
    ok = (got == want)
    print('[%s] qcc=%r oracle=%r' % ('PASS' if ok else 'FAIL', got, want))
    print('==== check_longlong_ptr: %s ====' % ('PASS' if ok else 'REPRO'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
