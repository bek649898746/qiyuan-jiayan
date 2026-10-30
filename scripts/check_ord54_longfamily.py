# -*- coding: utf-8 -*-
"""ORD-54 判据：审计施工单《long 家族两缺陷 + 多 obj 场景异常》的 D107 / D108 复现器。

来源 = `_施工单_long家族两缺陷与多obj异常_启元.md`（启元 · 2026-10-31 · D107/D108/D109）。

D107：`long*` / `unsigned long*` 形参的 `p[i]` 按 **8 字节**步长寻址（Windows LLP64 应 4）。
D108：`long` 家族 **转型常量在比较上下文** 按 64 位符号扩展（赋值/单独求值正确）。
D109：多 obj 链接场景返回值错误（未最小化，本判据不覆盖，台账注明）。

判据 = 编译 + **运行**，与 **gcc 同源 oracle** 逐值比对；gcc 不可用则退化为「期望常量」。
红 = 任一观测与 oracle 不一致（施工单两个缺陷都真实存在 ⟹ 当前应红）。
"""
import argparse
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord54')

D107 = """#include <stdio.h>
static long sum_l (long *p, int n) { long s = 0; int i; for (i = 0; i < n; i++) s += p[i]; return s; }
static unsigned long sum_ul (unsigned long *p, int n) { unsigned long s = 0; int i; for (i = 0; i < n; i++) s += p[i]; return s; }
int main (void) {
  long a[3]; unsigned long b[3];
  a[0] = 1; a[1] = 2; a[2] = 3;
  b[0] = 10; b[1] = 20; b[2] = 30;
  printf("D107 %ld %lu\\n", sum_l(a, 3), sum_ul(b, 3));
  return 0;
}
"""

D108 = """#include <stdio.h>
int main (void) {
  unsigned long a = 4294967295UL;
  unsigned long k = (unsigned long)-1L;
  printf("D108 %d %d %d %d %d %lu\\n", a == k, a == 4294967295UL, a == (unsigned long)-1L,
         a != (unsigned long)-1L, a < (unsigned long)-1L, (unsigned long)-1L);
  return 0;
}
"""

CASES = [
    ('D107', D107, 'D107 6 60'),
    ('D108', D108, 'D108 1 1 1 0 0 4294967295'),
]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8',
                          errors='replace', **kw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[SKIP] 编译器不存在: %s' % cc)
        return 0
    os.makedirs(OUT, exist_ok=True)
    gcc = None
    for cand in ('gcc', r'C:\msys64\ucrt64\bin\gcc.exe', r'C:\mingw64\bin\gcc.exe'):
        try:
            if run([cand, '--version'], timeout=20).returncode == 0:
                gcc = cand
                break
        except Exception:
            continue
    bad = 0
    for name, src, want in CASES:
        c = os.path.join(OUT, name + '.c')
        exe = os.path.join(OUT, name + '.exe')
        with open(c, 'w', encoding='utf-8', newline='\n') as f:
            f.write(src)
        r = run([cc, c, '-o', exe], cwd=REPO)
        if r.returncode != 0:
            print('[FAIL] %s 编译 rc=%d %s' % (name, r.returncode,
                                               ((r.stderr or '').strip().splitlines() or [''])[0][:90]))
            bad += 1
            continue
        q = run([exe])
        got = (q.stdout or '').strip().replace('\r', '')
        oracle = want
        gnote = '(期望常量)'
        if gcc:
            gx = os.path.join(OUT, name + '_gcc.exe')
            if run([gcc, '-O0', c, '-o', gx]).returncode == 0:
                g = run([gx])
                oracle = (g.stdout or '').strip().replace('\r', '')
                gnote = '(gcc oracle)'
        ok = (got == oracle)
        if not ok:
            bad += 1
        print('[%s] %-5s qcc=%r %s oracle=%r' % ('PASS' if ok else 'FAIL', name, got, gnote, oracle))
    print('==== check_ord54_longfamily: %s (%d/%d) ====' %
          ('PASS' if bad == 0 else 'FAIL', len(CASES) - bad, len(CASES)))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
