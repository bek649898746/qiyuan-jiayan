#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_macro_order —— 预处理 的「定义顺序 / 重定义」语义（2026-10-23 虾米 · R802/R803）。

三个载体，全部来自 sqlite3.c 的真实形态（`VVA_ONLY` + `#if !defined(NDEBUG)`）：

  A **顺序泄漏（正锚）**：`#if !defined(ZZZ)` 出现在 `#define ZZZ 1` **之前** ⟹ 必须走进真分支
    （G 展开为空 ⟹ 调试代码不参与编译 ⟹ rc=0）。修前：全表查 ⟹ 判成「已定义」⟹ 取 #else ⟹ 编译失败。
  B **重定义（C 6.10.7）**：`#define F(X) X` 之后 `#define F(X)`（空体）⟹ **最后定义生效**
    ⟹ F(...) 展开为空 ⟹ rc=0。修前：取首个 ⟹ 旧体复活 ⟹ 失败。
  C **顺序泄漏（负锚）**：`#if defined(ZZZ)` 在定义**之前** ⟹ 必须判假 ⟹ 走 #else 的 `X` 体
    ⟹ 故意访问不存在的字段 ⟹ **必须编译失败**（若通过 = 顺序判据失效 = 假绿）。

用法: python scripts/check_macro_order.py [--cc qcc_zh_seed.exe]
退出: 0 全过 / 1 有红 / 2 缺件
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TAIL = '''struct W { int a; };
int main(void){ struct W w; w.a = 0; %s return w.a; }
'''

CASES = [
    ('A-order-before-define', '#if !defined(ZZZ)\n# define G(X)\n#else\n# define G(X) X\n#endif\n#define ZZZ 1\n'
                              + (TAIL % 'G( w.b = 1; )'), 0),
    ('B-redefine-empty', '#define F(X) X\n#define F(X)\n' + (TAIL % 'F( w.b = 1; )'), 0),
    ('C-order-neg', '#if defined(ZZZ)\n# define G(X)\n#else\n# define G(X) X\n#endif\n#define ZZZ 1\n'
                    + (TAIL % 'G( w.b = 1; )'), 1),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[SKIP] 编译器不存在: %s' % cc); return 2
    tmp = tempfile.mkdtemp(prefix='morder_')
    bad = 0
    for name, src, want_rc in CASES:
        c = os.path.join(tmp, name + '.c')
        exe = os.path.join(tmp, name + '.exe')
        with open(c, 'w', encoding='utf-8') as f:
            f.write(src)
        p = subprocess.run([cc, '-o', exe, c], capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=60)
        rc = 0 if p.returncode == 0 else 1
        detail = ''
        if rc != want_rc:
            detail = ((p.stdout or '') + (p.stderr or '')).strip().split('\n')[0][:110]
        print('[%-22s] %s (rc=%d 期望 %d) %s' % (name, 'PASS' if rc == want_rc else 'FAIL', rc, want_rc, detail))
        if rc != want_rc:
            bad += 1
    print('=== 宏顺序/重定义 判据: %s ===' % ('全过' if bad == 0 else 'RED %d/%d' % (bad, len(CASES))))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
