#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_union_dim —— 数组维度 / sizeof 使解析器死循环或静默错值的四个真缺陷（2026-10-23 虾米）。

四个载体都从 sqlite3.c 的 `struct Bitvec` / `struct KeyInfo` 一族缩出来：

  A `feat_zh_union_dim_sizeof.c` — 匿名 union 里 `unsigned char a[(512/sizeof(unsigned int))*sizeof(unsigned int)]`
    ⟹ 修前 **A9001 字段名 'sizeof' 是保留词 (R472 误报)**（维度表达式被当字段继续解析）。
  B `feat_zh_union_dim_hang.c` — 具名 union 嵌在 struct 里 + `int a[(8/sizeof(int))*sizeof(int)]`
    ⟹ 修前 **编译器挂起（>60s 死循环）**，P0。
  C `feat_zh_union_dim_typedef.c` — `尺寸(<不完整 typedef> *)` 在字段维度里
    ⟹ 修前 求值器 不消费词位 ⟹ 静默错值 + A9001；**断言布局值 520**（不只「能编译」）。
  D `feat_zh_dim_typedef_sizeof.c` — `typedef 构 Key Key;` 先于 定义 ⟹ 型定表 尺寸=0 快照
    ⟹ 修前 `尺寸(Key)` 误报 A1001 不完整；**断言 8**。

判据：四个载体都必须在 **20 秒内 rc=0**（C/D 的 rc=0 即「布局值等于断言值」）。超时也算红。

用法: python scripts/check_union_dim.py [--cc qcc_zh_seed.exe]
退出: 0 全过 / 1 有红 / 2 缺件
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES = ['feat_zh_union_dim_sizeof.c', 'feat_zh_union_dim_hang.c',
         'feat_zh_union_dim_typedef.c', 'feat_zh_dim_typedef_sizeof.c']
TIMEOUT = 20


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[SKIP] 编译器不存在: %s' % cc); return 2
    tmp = tempfile.mkdtemp(prefix='udim_')
    bad = 0
    for name in CASES:
        src = os.path.join(REPO, 'scripts', name)
        if not os.path.exists(src):
            print('[SKIP] 载体缺失: %s' % name); bad += 1; continue
        exe = os.path.join(tmp, name + '.exe')
        try:
            p = subprocess.run([cc, '-o', exe, src], capture_output=True, text=True,
                               encoding='utf-8', errors='replace', timeout=TIMEOUT)
            ok = (p.returncode == 0)
            detail = '' if ok else ((p.stdout or '') + (p.stderr or '')).strip().split('\n')[0][:120]
            if ok:
                r = subprocess.run([exe], capture_output=True, timeout=TIMEOUT)
                ok = (r.returncode == 0)
                detail = '' if ok else '布局断言 FAIL (rc=%d)' % r.returncode
        except subprocess.TimeoutExpired:
            ok, detail = False, 'HANG >%ds（解析器死循环）' % TIMEOUT
        print('[%-30s] %s %s' % (name, 'PASS' if ok else 'FAIL', detail))
        if not ok:
            bad += 1
    print('=== union/维度 sizeof 判据: %s ===' % ('全过' if bad == 0 else 'RED %d/%d' % (bad, len(CASES))))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
