#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_alloc_fail —— R797 判据：全局表分配失败必须响亮退出，不许把 0 当表基址写 NULL。

修前（R797 现场）：`_va_alloc`/`清零分配` 失败返回 0，调用方直接 `表[idx] = ...`
⟹ 写 NULL ⟹ `0xC0000005`（并发/内存压力下才会触发，10-13 12 路扫 ffmpeg 事故）。

判据（真实入口，不 mock）：
  ① 对照：不设注入，编译最小源 rc=0；
  ② 注入：`QCC_FI_ALLOC=<序号>` 使第 N 个表分配返回 0（序号注入避开 env 编码差异）；
     断言 rc != 0 且 **不是** 0xC0000005（-1073741819 / 3221225477）；
  ③ stderr 含「全局表分配失败」与**表名**（--name 可钉死；默认空 = 只要求点名非空表名 ——
     序号 0 是**哪张**表属实现细节：2026-10-21 把 词型/词值/词名/词行/词列/词段/节名 七张表
     迁到静态区后，序号 0 变成 词无符号表，钉死表名 = 把「响亮拒绝」这条纪律绑到布局上）。
用法: python scripts/check_alloc_fail.py [--cc qcc_zh_v1.exe] [--index 0] [--name <表名>]
退出码: 0 = PASS；1 = RED
"""
import argparse
import os
import subprocess
import sys
import tempfile
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

SRC = '''#包含 <stdio.h>
函数 整 主(空) { 整 x = 1 + 2; 返 x - 3; }
'''


def run(cmd, env=None, timeout=120):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout,
                          encoding='utf-8', errors='replace', env=env)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--index', default='0')
    ap.add_argument('--name', default='')
    a = ap.parse_args()
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo)
    tmp = tempfile.mkdtemp(prefix='allocfail_')
    src = os.path.join(tmp, 'probe.c')
    with open(src, 'w', encoding='utf-8', newline='\n') as f:
        f.write(SRC)

    # ① 对照
    c0 = run([a.cc, src, '-o', os.path.join(tmp, 'ok.exe')])
    print('[①] 对照 compile rc=%d' % c0.returncode)
    if c0.returncode != 0:
        print('=== allocfail: RED (对照 编译失败) ===')
        return 1

    # ② 注入（序号）
    env = os.environ.copy()
    env['QCC_FI_ALLOC'] = str(a.index)
    c1 = run([a.cc, src, '-o', os.path.join(tmp, 'fail.exe')], env=env)
    out = (c1.stdout or '') + (c1.stderr or '')
    print('[②] 注入 index=%s compile rc=%d' % (a.index, c1.returncode))
    print('[③] output=%r' % out[:400])

    ok = True
    if c1.returncode == 0:
        print('  - 注入后仍 rc=0（分配失败被吞）')
        ok = False
    if c1.returncode in (-1073741819, 3221225477):
        print('  - 注入后写 NULL 崩溃 (0xC0000005)')
        ok = False
    if '全局表分配失败' not in out:
        print('  - 缺「全局表分配失败」点名诊断')
        ok = False
    if a.name:
        if a.name not in out:
            print('  - 诊断未点名表 %s' % a.name)
            ok = False
    else:
        import re
        m = re.search(r'全局表分配失败:\s*(\S+)', out)
        if not m:
            print('  - 诊断未点名任何表（须 `全局表分配失败: <表名>`）')
            ok = False
        else:
            print('[④] 点名的表 = %s（序号 %s）' % (m.group(1), a.index))

    print()
    if ok:
        print('=== allocfail: PASS ===')
        return 0
    print('=== allocfail: RED ===')
    return 1


if __name__ == '__main__':
    sys.exit(main())
