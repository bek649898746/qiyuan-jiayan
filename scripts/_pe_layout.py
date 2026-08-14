#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""解析 PE: 输出 IMAGE_BASE、节表、.data raw 首4字节(heap_start)、.text 末尾 CRT stub 的 stk_top."""
import struct, sys

def parse_pe(path):
    with open(path, 'rb') as f:
        d = f.read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    assert d[pe:pe+4] == b'PE\x00\x00', 'not PE'
    nsec = struct.unpack_from('<H', d, pe+6)[0]
    optsz = struct.unpack_from('<H', d, pe+20)[0]
    magic = struct.unpack_from('<H', d, pe+24)[0]
    image_base = 0
    if magic == 0x20b:
        image_base = struct.unpack_from('<Q', d, pe+24+24)[0]
    else:
        image_base = struct.unpack_from('<I', d, pe+24+28)[0]
    sec_off = pe + 24 + optsz
    print(f'{path}: PE magic={magic:#x} image_base={image_base:#x} sections={nsec}')
    sects = []
    for i in range(nsec):
        o = sec_off + i*40
        name = d[o:o+8].rstrip(b'\x00').decode('latin1')
        vsize, vaddr, rsize, roff = struct.unpack_from('<IIII', d, o+8)
        sects.append((name, vaddr, vsize, roff, rsize))
        print(f'  .{name:8s} va={vaddr:#x} vsize={vsize:#x} raw_off={roff:#x} raw_size={rsize:#x}')
    return d, image_base, sects

def main(path):
    d, image_base, sects = parse_pe(path)
    byname = {}
    for n, vaddr, vsize, roff, rsize in sects:
        byname[n.strip('.')] = (vaddr, vsize, roff, rsize)
    if 'data' in byname:
        vaddr, vsize, roff, rsize = byname['data']
        heap_start = struct.unpack_from('<I', d, roff)[0]
        print(f'.data raw first 4 bytes = heap_start = {heap_start:#x}')
        stc_n_est = (heap_start - image_base - vaddr - 0x300 - 2560) // 4
        print(f'  -> 反推 stc_n ~ {stc_n_est}  (4*stc_n = {4*stc_n_est:#x})')
        print(f'  -> heap 区间 [ {heap_start:#x}, ... )')
    if 'text' in byname:
        vaddr, vsize, roff, rsize = byname['text']
        txt = d[roff:roff+rsize]
        # CRT stub 第一条指令 mov esp,imm32 = BC imm32 (mov_ri_ext reg=4, 0xB8|4)。
        # 注意 41 BC imm32 = mov r12d,imm32 (argv_va), 必须排除 (要求前一个字节 != 0x41)。
        i = len(txt) - 1
        found = False
        while i > 0:
            if txt[i] == 0xBC and i + 5 <= len(txt) and txt[i-1] != 0x41:
                imm = struct.unpack_from('<I', txt, i+1)[0]
                if 'data' in byname:
                    dv, dvs, dro, drs = byname['data']
                    if dv <= imm - image_base < dv + dvs:
                        print(f'CRT stub @ text_off={roff+i:#x} rva={vaddr+i:#x}: mov esp, {imm:#x}  (stk_top)')
                        if 'data' in byname:
                            print(f'  stk_top - heap_start = {imm - heap_start:#x} ({(imm-heap_start)/1024/1024:.2f} MB)')
                        found = True
                        break
            i -= 1
        if not found:
            print('(未在 .text 找到合法的 mov esp,stk_top)')

if __name__ == '__main__':
    main(sys.argv[1])
