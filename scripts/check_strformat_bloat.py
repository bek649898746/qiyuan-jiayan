#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_strformat_bloat —— lua `str_format` 单函数体被**复制**成 ~25 份的体积判据。

现象（2026-11-10 实测，qcc_zh_v2.exe = D2E3462B）：
  `-c lstrlib.c` 里 `str_format` = **82,846 B**（同文件 gcc -O2 整份才 32,589 B），
  其汇编 21,471 行，`.L` 跳转目标 2,102 个；同一段「while 循环头 + 一个 case 体」
  以 ~206 行的周期**重复 25 次**（每份自带 `跳转 .L<自己>` 回边）。
  ⟹ lstrlib.c 8.57× / lobject.c 9.00× 超 5× 的**唯一**来源（两文件在 HEAD 与本次刀前后
      **逐字节相同**，不是 printf 格式块通道）。

判据：
  A `str_format` 体积 ≤ 16,384 B（今天 82,846 ⟹ 红；把 25 份收敛成 1 份后应 ≈3–5 KB）；
  B 同文件 `lstrlib.c -c` 总字节 ≤ 131,072 B（今天 279,213 ⟹ 红；gcc -O2 32,589）；
  C 该文件仍能 `-c` 成功（不是靠报错变小）。

用法: python scripts/check_strformat_bloat.py [--cc qcc_zh_v2.exe] [--lua <dir>]
退出码: 0 = PASS / 1 = FAIL / 2 = 环境未就绪
"""
import argparse
import io
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIM_FN = 16384
LIM_FILE = 131072


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    ap.add_argument('--lua', default=os.environ.get('QCC_LUA_DIR', r'D:\qcc_corpus\lua-5.4.7\src'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    src = os.path.join(a.lua, 'lstrlib.c')
    if not os.path.exists(cc) or not os.path.exists(src):
        print('[环境未就绪] cc=%s src=%s' % (cc, src))
        return 2
    work = tmp_dir('strformat_bloat')
    obj = os.path.join(work, 'lstrlib.o')
    env = dict(os.environ)
    env['QCC_RT_PATH'] = os.path.join(ROOT, 'srclib', 'qcc_rt.c')
    env['QCC_DBG_FMAP'] = '1'
    r = subprocess.run([cc, src, '-c', '-I', a.lua, '-o', obj], cwd=ROOT,
                       capture_output=True, text=True, encoding='utf-8', errors='replace', env=env)
    if r.returncode != 0:
        print('[FAIL] lstrlib.c -c 失败 rc=%d\n%s' % (r.returncode, (r.stdout + r.stderr)[-400:]))
        return 1
    fsize = os.path.getsize(obj)
    sz = {}
    for line in ((r.stdout or '') + (r.stderr or '')).splitlines():
        m = re.match(r'\[FMAP\] (\S+) (\d+) (\d+)', line.strip())
        if m:
            sz[m.group(1)] = int(m.group(3)) - int(m.group(2))
    if 'str_format' not in sz:
        print('[FAIL] FMAP 里没有 str_format（QCC_DBG_FMAP 未生效？）')
        return 1
    fn = sz['str_format']
    print('lstrlib.c -c 总 %d B（上限 %d）' % (fsize, LIM_FILE))
    print('str_format   %d B（上限 %d）；同文件函数数 %d' % (fn, LIM_FN, len(sz)))
    top = sorted(sz.items(), key=lambda kv: -kv[1])[:5]
    for n, v in top:
        print('   %-14s %8d B' % (n, v))
    bad = []
    if fn > LIM_FN:
        bad.append('str_format %d B > %d' % (fn, LIM_FN))
    if fsize > LIM_FILE:
        bad.append('lstrlib.c %d B > %d' % (fsize, LIM_FILE))
    print()
    if bad:
        print('[失败 FAIL] check_strformat_bloat：函数体被复制成多份')
        for b in bad:
            print('  - ' + b)
        return 1
    print('[通过 PASS] check_strformat_bloat：str_format %d B / 文件 %d B' % (fn, fsize))
    return 0


if __name__ == '__main__':
    sys.exit(main())
