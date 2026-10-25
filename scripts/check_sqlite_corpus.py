#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_sqlite_corpus —— SQLITE-CORPUS 的**解除判据**（2026-10-23 虾米）。

为什么要有它：台账里 `SQLITE-CORPUS` 是 blocked，按验收口径（criterion 3）blocked 行必须带
「解除条件 + 可复跑的解除判据 + 当前阻塞现场」。本脚本就是那条**可复跑的解除判据**：

  现在（blocked）：`qcc -c sqlite3.c` 必失败 —— 打印第一条 `[ERR]`（当前是
    `A9001 ... sqlite3_mutex 无字段 nRef`，且 diagnostics 行号落在注释行 = 量具失真）。
  修好后：`qcc -c sqlite3.c` rc=0 ⟹ 本脚本转绿 ⟹ 台账该行可以从 blocked 转 fixed。

判据只用**已发布的编译器**驱动真实语料，不自证、不硬编码结果（除「今天必须还红」这一条，
那正是 blocked 的现场本身）。

2026-10-23 修正调用：必须带 `-I compat_prelude.h`（sqlite 的 `assert()` 属 <assert.h>；
不带预置头时 assert 退化成函数调用，参数被真的解析 ⟹ `assert(p->nRef)` 打到只在
`#ifdef SQLITE_DEBUG` 下存在的字段 ⟹ 假 A9001）与 `-D _WIN32=1`（SQLITE_OS_WIN；与 curl 语料同一手法）。

用法: python scripts/check_sqlite_corpus.py [--cc qcc_zh_seed.exe]
退出: 0 = 已解除（能编译）；1 = 仍 blocked（附现场）；2 = 环境缺件
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORPUS = r'D:\qiyuan_home\corpus\sqlite-amalgamation-3530400\sqlite-amalgamation-3530400'
SRC = os.path.join(CORPUS, 'sqlite3.c')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')
    ap.add_argument('--ts0', action='store_true',
                    help='THREADSAFE=0 路径：只验证「静态函数表 fn_static 满」这道墙已不在（其余错误照报）')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[SKIP] 编译器不存在: %s' % cc); return 2
    if not os.path.exists(SRC):
        print('[SKIP] 语料不存在: %s' % SRC); return 2

    tmp = tempfile.mkdtemp(prefix='sqlite_')
    obj = os.path.join(tmp, 'sqlite3.o')
    prelude = os.path.join(REPO, 'compat_prelude.h')
    cmd = [cc, '-c', SRC, '-o', obj, '-I', prelude, '-I', CORPUS,
           '-D', '_WIN32=1',
           '-D', 'SQLITE_OMIT_LOAD_EXTENSION=1', '-D', 'SQLITE_OMIT_DEPRECATED=1']
    if a.ts0:
        cmd += ['-D', 'SQLITE_THREADSAFE=0']
    print('# SQLITE-CORPUS 解除判据  cc=%s' % os.path.basename(cc))
    p = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8',
                       errors='replace', timeout=900)
    out = ((p.stdout or '') + (p.stderr or '')).strip()
    first_err = [l for l in out.split('\n') if '[ERR]' in l]
    if a.ts0:
        # 2026-10-23: 净深一 1024 -> 8192 后，这条确定性拒绝不再出现；其余错误属别行
        gone = 'fn_static' not in out and '静态函数表' not in out
        print('[ts0] 静态函数表 fn_static 护栏已不在: %s' % ('PASS' if gone else 'FAIL'))
        for l in (first_err[:2] or ['(无 [ERR])']):
            print('   当前首个错误: ' + l.strip()[:160])
        return 0 if gone else 1
    if p.returncode == 0:
        print('[解除] sqlite3.c -c rc=0（%.1f MB）⟹ 本行可以从 blocked 转 fixed'
              % (os.path.getsize(obj) / 1048576.0))
        return 0
    print('[仍 blocked] rc=%d' % p.returncode)
    for l in (first_err[:3] or out.split('\n')[:3]):
        print('   ' + l.strip()[:200])
    print('# 解除条件（见 _TASKLIST.json 的 next_step）：行号量具 → 撞词收窄 → 平台宏（_WIN32 / SQLITE_OS_WIN）')
    return 1


if __name__ == '__main__':
    sys.exit(main())
