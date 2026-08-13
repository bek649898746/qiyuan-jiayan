# -*- coding: utf-8 -*-
import io, sys, struct
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

def dump_coff_syms(path, want):
    b = open(path, 'rb').read()
    nsym = struct.unpack_from('<I', b, 12)[0]
    symptr = struct.unpack_from('<I', b, 8)[0]
    # 字符串表
    strtab = None
    if symptr > 0 and nsym > 0:
        st_off = symptr + nsym * 18
        stl = struct.unpack_from('<I', b, st_off)[0]
        if stl > 4:
            strtab = b[st_off:st_off+stl]
    for i in range(nsym):
        sy = b[symptr + i*18: symptr + i*18 + 18]
        # 符号名
        if struct.unpack_from('<I', sy, 0)[0] == 0:
            off = struct.unpack_from('<I', sy, 4)[0]
            name = strtab[off:strtab.find(b'\0', off)].decode('utf-8', 'replace') if strtab else ''
        else:
            name = sy[0:8].split(b'\0')[0].decode('utf-8', 'replace')
        value = struct.unpack_from('<I', sy, 8)[0]
        sec = struct.unpack_from('<h', sy, 12)[0]
        sc = sy[16]
        aux = sy[17]
        if name == want:
            print(f'{want}: value={value} sec={sec} sc={sc} aux={aux}')
        if aux > 0:
            i += aux

dump_coff_syms(r'C:\Users\Administrator\Desktop\git-2.45.2\_objs\abspath.o', 'default_swab32')
dump_coff_syms(r'C:\Users\Administrator\Desktop\git-2.45.2\_objs\abspath.o', '_default_swab32')
dump_coff_syms(r'C:\Users\Administrator\Desktop\git-2.45.2\_objs\abspath.o', 'default_swab16')
