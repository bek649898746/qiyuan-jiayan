# -*- coding: utf-8 -*-
"""ORD-42 判据：`(enum T)` / `(enum T*)` 转型 —— **编译并运行产物**，断言运行值。

为什么必须跑产物（skeptic 2 打回）：只看编译 rc 会把「类型码打错、运行读垃圾/写崩」判成 fixed。
覆盖：
  ① 值转型：`(整)(枚 色)1` → 1
  ② 指针读：`(整)(*(枚 色*)&x)` 且 x=7 → 7
  ③ 指针写：`*(枚 色*)&y = 7` 且 y 初值 42 → y == 7
（读/写都必须走整数 4B；浮点指针码会让读回垃圾、写崩 0xC0000005）
用法: python scripts/check_ord33b_enumcast.py [--cc qcc_zh_v1.exe]
"""
import argparse
import io
import os
import subprocess
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord33b')

SRC = '\n'.join([
    '#包含 <stdio.h>',
    '枚 色 { 红 = 1 };',
    '函数 整 主(空){',
    '    整 值 = (整)(枚 色)1;',
    '    整 x = 7;',
    '    整 读 = (整)(*(枚 色*)&x);',
    '    整 y = 42;',
    '    *(枚 色*)&y = 7;',
    '    格式化 "ENUMRT %d %d %d\\n", 值, 读, y;',
    '    返 0;',
    '}',
    '',
])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, 'enumrt.jy')
    exe = os.path.join(TMP, 'enumrt.exe')
    io.open(p, 'w', encoding='utf-8').write(SRC)
    r = subprocess.run([cc, p, '-o', exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if r.returncode != 0:
        print('[FAIL] 编译失败: %s' % ((r.stderr or '').strip().splitlines() or [''])[0][:110])
        return 1
    q = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace')
    out = (q.stdout or '').strip()
    print('[RUN] rc=%d out=%r' % (q.returncode, out))
    if q.returncode != 0:
        print('[REPRO] 产物运行崩溃/非零退出（指针 读/写 通道错）⟹ blocked 诚实')
        return 1
    if out != 'ENUMRT 1 7 7':
        print('[REPRO] 运行值不符（应 ENUMRT 1 7 7）⟹ blocked 诚实')
        return 1
    print('[PASS] 值转型 1 · 指针读 7 · 指针写 7 ⟹ 应转 fixed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
