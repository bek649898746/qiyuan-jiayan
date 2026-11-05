# -*- coding: utf-8 -*-
"""lua 5.4.7 → ELF 目标文件（-c --target=elf）批量编译。

源码是**仓库内固定副本** tests/corpus/lua-5.4.7/src（不依赖会被清理的临时目录）。
默认产物：D:\\qcc_tmp\\lua_elf\\*.o；链接与运行见 --link 帮助。

用法:
    python tests/corpus/compile_lua_elf.py                # 编 33 个核心 .c
    python tests/corpus/compile_lua_elf.py --cc qcc_zh_v1.exe --out D:\\qcc_tmp\\lua_elf
退出码: 0 = 全部 rc=0；1 = 有文件失败（fail-closed）。
"""
import argparse
import glob
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))          # qiyuan-jiayan/
SRC = os.path.join(HERE, 'lua-5.4.7', 'src')
EXCLUDE = ('luac.c', 'onelua.c')                        # 工具 + 聚合文件，非核心


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe', help='编译器（默认自举最终产物 v2）')
    ap.add_argument('--out', default=r'D:\qcc_tmp\lua_elf')
    a = ap.parse_args()

    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('missing compiler:', cc)
        return 1
    os.makedirs(a.out, exist_ok=True)

    files = [f for f in sorted(glob.glob(os.path.join(SRC, '*.c')))
             if os.path.basename(f) not in EXCLUDE]
    bad = 0
    t0 = time.time()
    for f in files:
        base = os.path.basename(f)
        o = os.path.join(a.out, base + '.o')
        p = subprocess.run([cc, '-c', '--target=elf', f, '-o', o], cwd=ROOT,
                           capture_output=True, text=True, encoding='utf-8', errors='replace')
        if p.returncode != 0:
            bad += 1
            print('%-16s FAIL rc=%d' % (base, p.returncode))
            print((p.stdout or '') + (p.stderr or ''))
        else:
            print('%-16s OK' % base)
    print('\ncore=%d bad=%d wall=%.1fs' % (len(files), bad, time.time() - t0))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
