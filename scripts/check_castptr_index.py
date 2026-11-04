# -*- coding: utf-8 -*-
"""check_castptr_index —— 转型表达式作基的指针算术（`(T*)x + i` / `(T*)x[i]`）复现器。

由来（2026-10-30 · ORD-20261030-65）：修 D107/D108 时发现 —— **内联转型表达式**的下标/加法
步长仍是 8，而把同一转型**赋给变量**再下标则正确：

    long a[2]; a[0]=1; a[1]=2; void *vp = a;
    u1 = ((long *)vp)[1];      -> 甲言 0（gcc 2）   ← 错
    long *lp = (long *)vp; u2 = lp[1];  -> 甲言 2（gcc 2） ✓
    u3 = *(((long *)vp) + 1);  -> 甲言 0（gcc 2）   ← 错

根因（插桩实证）：`符六` 已正确算出 4（`[FU6] 深己=1 符六=4`），并写入 `指元素[转型节点]`；
但**下游的指针算术/下标缩放没有消费它**（变量形态经变量表消费，内联形态没有）。
判据：三形态 vs gcc 逐值一致；任一不一致 ⟹ 复现（rc=1）。
"""
from qcc_tmp import tmp_dir
import argparse
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or tmp_dir('castptr')

SRC = """#include <stdio.h>
int main (void) {
  long a[2]; a[0] = 1; a[1] = 2;
  void *vp = a;
  long u1 = ((long *)vp)[1];
  long *lp = (long *)vp;
  long u2 = lp[1];
  long u3 = *(((long *)vp) + 1);
  printf("%ld %ld %ld\\n", u1, u2, u3);
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
    q = run([exe])
    got = (q.stdout or '').strip()
    want = '2 2 2'
    gcc = None
    for cand in ('gcc', r'C:\msys64\ucrt64\bin\gcc.exe', r'C:\mingw64\bin\gcc.exe'):
        if run([cand, '--version']).returncode == 0:
            gcc = cand
            break
    if gcc:
        g = os.path.join(TMP, 'g.exe')
        if run([gcc, '-O0', '-w', src, '-o', g]).returncode == 0:
            want = (run([g]).stdout or '').strip()
    ok = (got == want)
    print('[%s] qcc=%r oracle=%r' % ('PASS' if ok else 'FAIL', got, want))
    print('==== check_castptr_index: %s ====' % ('PASS' if ok else 'REPRO'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
