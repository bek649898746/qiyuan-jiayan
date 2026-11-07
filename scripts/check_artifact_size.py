#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_artifact_size —— ORD-20261107-138 复现器：最小程序的产物 99.9% 是运行库死代码。

缺陷（审计递单 2026-11-07）：`整 主(空){ 返 0; }` 编出 385,536 B，其中 99.9% 是
运行库死代码（184 个函数只用到 1 个）；gcc 同程序 15,768 B（甲言 = 24.5×）。

本判据量**最小程序产物体积**，并以 `gcc -O0` 同程序为参照。
阈值 5×（当前 24.5×）—— 红线本身属口径裁定；在红线定下来之前，本判据只负责
「把这个数说出来并判红」，不做设计决策。

退出码：0 = 已在红线内；1 = 仍超（预期）。
"""
import io
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = os.path.join(ROOT, 'qcc_zh_v1.exe')
SRC = '整 主(空) { 返 0; }\n'
LIMIT_X = 5.0


def main():
    work = tmp_dir('artsize')
    try:
        src = os.path.join(work, 'min.c')
        exe = os.path.join(work, 'min.exe')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
        r = subprocess.run([CC, src, '-o', exe], cwd=ROOT, capture_output=True)
        if r.returncode != 0 or not os.path.exists(exe):
            print('[FAIL] 最小程序编译失败 rc=%d' % r.returncode)
            return 1
        jy = os.path.getsize(exe)

        gcc = None
        for cand in ('gcc', 'x86_64-w64-mingw32-gcc'):
            q = subprocess.run([cand, '--version'], capture_output=True)
            if q.returncode == 0:
                gcc = cand
                break
        base = None
        if gcc:
            csrc = os.path.join(work, 'min_gcc.c')
            cexe = os.path.join(work, 'min_gcc.exe')
            io.open(csrc, 'w', encoding='utf-8', newline='\n').write('int main(void){return 0;}\n')
            q = subprocess.run([gcc, '-O0', csrc, '-o', cexe], capture_output=True)
            if q.returncode == 0 and os.path.exists(cexe):
                base = os.path.getsize(cexe)

        if base:
            ratio = jy / base
            print('[量] 甲言 %d B · gcc -O0 %d B ⟹ %.1f×（红线 %.1f×）' % (jy, base, ratio, LIMIT_X))
            if ratio > LIMIT_X:
                print('[RED] ORD-20261107-138：最小程序仍是 gcc 的 %.1f 倍' % ratio)
                return 1
            print('[PASS] 体积已在红线内（%.1f×）' % ratio)
            return 0

        print('[量] 甲言 %d B（无 gcc 参照）' % jy)
        if jy > 128 * 1024:
            print('[RED] ORD-20261107-138：最小程序 %d B > 128 KB' % jy)
            return 1
        return 0
    finally:
        import shutil
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
