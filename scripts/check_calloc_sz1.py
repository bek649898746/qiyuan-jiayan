# -*- coding: utf-8 -*-
"""check_calloc_sz1 —— **`calloc(n, 1)` 不许偶发崩溃**（G-P0-CALLOC 判据，proxy）。

现场（2026-10-24，`D:\\_corpus_bench_20261022\\probe\\callocbug`）：
  `void *p = calloc(8, 1); printf; free(p);` ⟹ 20–30 次里 **40–70%** 崩 `0xC0000005`；
  `calloc(8, sizeof(int))`（sz>1）0/15 崩；gcc 两版全 0%。
  ⟹ 与第二实参 `sz == 1` 相关，属确定性代码缺陷 + 运行时布局抖动。

判据（proxy，两次断言都必须成立）：
  ① **行为**：`calloc(8,1)` 探针连跑 20 次，**任一次非 0 退出即红**
     （p≈0.5 时漏判概率 ≈ 1e-6，足以当确定性判据）；
  ② **结构**：`-S` 里 calloc 降级序列必须存在且能定位（`乘64 r10, r2` 附近有 `加64 r11, r10`
     与 `存64 [r8], r10`（头 = n*sz，R821d 修正）），否则说明降级路径漂移，判据需要重写 —— 同样判红。

用法: python scripts/check_calloc_sz1.py [--cc qcc_zh_seed.exe] [--runs 20]
退出码: 0 = 20/20 全 0 退出且结构可定位；1 = 有崩溃或结构漂移（缺陷仍在）
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = '#include <stdlib.h>\n#include <stdio.h>\nint main(void){void*p=calloc(8,1);printf("x\\n");free(p);return 0;}\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    ap.add_argument('--runs', type=int, default=20)
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        # 2026-10-24（判据方打回）: 缺主体不得当绿 —— rc=2 = 环境未就绪，不是 PASS
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    w = tempfile.mkdtemp(prefix='qcc_calloc_')
    src = os.path.join(w, 'c1.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
    exe = os.path.join(w, 'c1.exe')
    r = subprocess.run([cc, src, '-o', exe], capture_output=True, timeout=300)
    if r.returncode != 0:
        # 2026-10-24: 探针源码编译失败 = 判据本身坏了，不能当绿
        print('[FAIL] 探针编译失败 rc=%d（未取到现场，不判绿）' % r.returncode)
        return 1
    crashes = 0
    for _ in range(a.runs):
        q = subprocess.run([exe], capture_output=True, timeout=60)
        if q.returncode != 0:
            crashes += 1
    print('[behavior] %d/%d 次崩溃' % (crashes, a.runs))

    asm = os.path.join(w, 'c1.exe.asm')
    subprocess.run([cc, '-S', src, '-o', exe], capture_output=True, timeout=300)
    text = io.open(asm, encoding='utf-8', errors='replace').read() if os.path.exists(asm) else ''
    site = '乘64 r10, r2' in text and '加64 r11, r10' in text and '存64 [r8], r10' in text
    print('[structure] calloc 降级序列可定位=%s' % site)

    ok = crashes == 0 and site
    print('=== calloc(sz=1): %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
