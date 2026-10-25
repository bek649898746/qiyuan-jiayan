# -*- coding: utf-8 -*-
"""check_coff_symbols —— **两条 COFF 缺陷**（台账 D54 第①步 · 2026-10-25 启元递刀）。

审计实测（`dumpbin /symbols`、`/headers`）：
  ① `main` 符号被标 **Static**（StorageClass=3），而 C 链接约定要求 **External**（=2）
     ⟹ 外部链接器/调试器把它当局部符号；`/ENTRY` 与 PDB 的 `S_PUB32` 都拿不到。
  ② 产物含 **重复 `.data` 段** ⟹ 链接器按“同节名归并”时行为不确定（体积/寻址受影响）。

判据（自包含，不依赖 dumpbin）：
  ① `-c` 产出的 COFF 里，`main`/`主` 的 StorageClass == 2（EXTERNAL）
  ② 节表里 `.data` 只出现 **一次**
用法: python scripts/check_coff_symbols.py [--cc qcc_zh_seed.exe]
退出码: 0 = 两条都符；1 = 有缺陷；2 = 环境未就绪（不判绿）
"""
import argparse
import io
import os
import struct
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = 'int main(void){ return 0; }\n'


def parse_coff(path):
    b = open(path, 'rb').read()
    if len(b) < 20:
        return None
    machine, nsec, tstamp, symptr, nsym, optsz, chars = struct.unpack_from('<HHIIIHH', b, 0)
    if machine not in (0x8664, 0x14c):
        return None
    secs = []
    off = 20 + optsz
    for i in range(nsec):
        s = off + 40 * i
        if s + 40 > len(b):
            break
        name = b[s:s + 8].split(b'\0')[0].decode('latin1')
        secs.append(name)
    syms = []
    stroff = symptr + 18 * nsym
    strsz = struct.unpack_from('<I', b, stroff)[0] if stroff + 4 <= len(b) else 4
    strtab = b[stroff:stroff + strsz]
    for i in range(nsym):
        s = symptr + 18 * i
        if s + 18 > len(b):
            break
        name_raw = b[s:s + 8]
        if name_raw[:4] == b'\0\0\0\0':
            o = struct.unpack_from('<I', name_raw, 4)[0]
            name = strtab[o:strtab.find(b'\0', o)].decode('utf-8', 'replace') if o < len(strtab) else ''
        else:
            name = name_raw.split(b'\0')[0].decode('latin1')
        sclass = b[s + 16]
        syms.append((name, sclass))
    return secs, syms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    w = tempfile.mkdtemp(prefix='qcc_coff_')
    src = os.path.join(w, 'm.c')
    obj = os.path.join(w, 'm.obj')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
    r = subprocess.run([cc, '-c', src, '-o', obj], capture_output=True, timeout=600)
    if r.returncode != 0:
        print('[RED] `-c` 编译失败 rc=%d | %s'
              % (r.returncode, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace').strip()[:140]))
        print('=== COFF 符号: FAIL（编不出 obj）===')
        return 1
    p = parse_coff(obj)
    if not p:
        print('[RED] obj 不是可解析的 COFF（machine 不在 0x8664/0x14c）')
        print('=== COFF 符号: FAIL ===')
        return 1
    secs, syms = p
    main_sc = None
    for n, sc in syms:
        if n in ('main', '_main', '主'):
            main_sc = sc
            break
    ok1 = (main_sc == 2)
    print('[%s] ① main 符号 StorageClass=%s（期望 2=EXTERNAL）'
          % ('OK' if ok1 else 'RED', main_sc))
    ndata = sum(1 for s in secs if s == '.data')
    ok2 = (ndata == 1)
    print('[%s] ② 节表 .data 出现 %d 次（期望 1）｜ 节: %s'
          % ('OK' if ok2 else 'RED', ndata, ','.join(secs)[:120]))
    ok = ok1 and ok2
    print('=== COFF 符号: %s ===' % ('PASS' if ok else 'FAIL（D54 第①步）'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
