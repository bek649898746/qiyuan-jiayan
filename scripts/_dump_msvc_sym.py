# -*- coding: utf-8 -*-
import io, sys, struct
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
b = open(r'C:\Users\Administrator\Desktop\git-2.45.2\_objs\compat_msvc.o', 'rb').read()
nsym = struct.unpack_from('<I', b, 12)[0]
symptr = struct.unpack_from('<I', b, 8)[0]
st_off = symptr + nsym * 18
stl = struct.unpack_from('<I', b, st_off)[0]
strtab = b[st_off:st_off+stl]
for i in range(nsym):
    sy = b[symptr+i*18:symptr+i*18+18]
    if struct.unpack_from('<I', sy, 0)[0] == 0:
        off = struct.unpack_from('<I', sy, 4)[0]
        name = strtab[off:strtab.find(b'\0', off)].decode('utf-8', 'replace')
    else:
        name = sy[0:8].split(b'\0')[0].decode('utf-8', 'replace')
    aux = sy[17]
    if aux > 0:
        i += aux
    if any(k in name for k in ['hash_algos', 'strvec', 'gettimeofday', 'the_hash_algo']):
        sec = struct.unpack_from('<h', sy, 12)[0]
        print(f'{name}: sc={sy[16]} sec={sec}')
