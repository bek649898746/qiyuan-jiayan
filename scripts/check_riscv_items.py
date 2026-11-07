#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_riscv_items —— ORD-20261104-120 三件主线可查项的机器判据。

① `静态数` 口径：槽数（每槽 4 字节），指针/可空值占 2 槽（8 字节）——
   源码断言三处（指针 +2 槽 / `4 * 静态数` / 容量守卫同口径）。
② `QCC_DBG_FMAP=1`：实跑编译器，断言输出 `[FMAP] 名 起 止` 且能定位 `主`/`main`。
③ 双后端守卫清单：grep「依赖 x86 指令长度/字节布局」的点，输出条目数（>0 即可）。

用法: python scripts/check_riscv_items.py [--cc qcc_zh_v1.exe]
退出码: 0 = 三件都可查/成立；1 = 有红
"""
import argparse
import io
import os
import re
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    text = io.open(SRC, 'rb').read().decode('utf-8', 'surrogateescape')
    red = []

    # ① static-slot accounting
    c1 = ('静态数 += (可空值静 || 指元素 > 0) ? 2 : 1;' in text)
    c2 = ('4 * 静态数' in text)
    c3 = ('静态数 >= 0x20000000' in text)
    ok1 = c1 and c2 and c3
    print('[%s] ① 静态数口径：指针 +2 槽=%s / 4*静态数=%s / 容量守卫=%s'
          % ('PASS' if ok1 else 'FAIL', c1, c2, c3))
    if not ok1:
        red.append('①')

    # ② FMAP
    with tempfile.TemporaryDirectory(prefix='qcc_fmap_') as tmp:
        src = os.path.join(tmp, 't.jy')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(
            '整 主(空) {\n    整 a = 1;\n    返 a;\n}\n')
        env = dict(os.environ)
        env['QCC_DBG_FMAP'] = '1'
        r = subprocess.run([cc, src, '-o', os.path.join(tmp, 't.exe')], cwd=ROOT, env=env,
                           capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=600)
        txt = (r.stdout or '') + (r.stderr or '')
        rows = re.findall(r'\[FMAP\]\s+(\S+)\s+(\d+)\s+(\d+)', txt)
        has_main = any(n in ('主', 'main') for n, _, _ in rows)
        mono = all(int(s) <= int(e) for _, s, e in rows)
        ok2 = r.returncode == 0 and len(rows) > 0 and has_main and mono
        print('[%s] ② FMAP：rc=%d 条目=%d 含主/main=%s 起<=止=%s'
              % ('PASS' if ok2 else 'FAIL', r.returncode, len(rows), has_main, mono))
        if not ok2:
            red.append('②')

    # ③ x86-layout guard inventory
    pats = [r'指令起\s*\+\s*指令长', r'码位\s*-\s*反高', r'文本基址\s*\+\s*码位']
    hits = sum(len(re.findall(p, text)) for p in pats)
    ok3 = hits > 0
    print('[%s] ③ 双后端守卫清单：x86 布局依赖点 %d 处' % ('PASS' if ok3 else 'FAIL', hits))
    if not ok3:
        red.append('③')

    if red:
        print('=== ORD-120 可查项: FAIL -> %s ===' % red)
        return 1
    print('=== ORD-120 可查项: PASS（口径正确 + FMAP 可用 + 守卫清单非空）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
