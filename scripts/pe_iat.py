# -*- coding: utf-8 -*-
"""解析 PE 的导入表：DLL 名 + 该 DLL 的函数名**顺序**（= 运行时填充 IAT 的顺序）
   用法: python pe_iat.py <exe>
"""
import struct
import sys


def rd(b, off, n):
    return b[off:off + n]


def u16(b, o):
    return struct.unpack_from('<H', b, o)[0]


def u32(b, o):
    return struct.unpack_from('<I', b, o)[0]


def cstr(b, o):
    e = b.index(b'\x00', o)
    return b[o:e].decode('ascii', 'replace')


def main():
    p = sys.argv[1]
    b = open(p, 'rb').read()
    pe = u32(b, 0x3C)
    assert rd(b, pe, 4) == b'PE\x00\x00'
    nsec = u16(b, pe + 6)
    optsz = u16(b, pe + 20)
    opt = pe + 24
    magic = u16(b, opt)
    pe32plus = (magic == 0x20b)
    print('magic=%#x (%s) sections=%d' % (magic, 'PE32+' if pe32plus else 'PE32', nsec))
    # DataDirectory[1] = Import Table
    dd = opt + (112 if pe32plus else 96)
    imp_rva, imp_sz = u32(b, dd + 8), u32(b, dd + 12)
    print('Import Directory RVA=%#x size=%d' % (imp_rva, imp_sz))

    # 节表
    secs = []
    so = opt + optsz
    for i in range(nsec):
        o = so + i * 40
        name = rd(b, o, 8).rstrip(b'\x00').decode('ascii', 'replace')
        vsz, va, rsz, ra = u32(b, o + 8), u32(b, o + 12), u32(b, o + 16), u32(b, o + 20)
        secs.append((name, va, vsz, ra, rsz))
        print('  sec %-8s VA=%#-8x vsz=%-8d raw=%#-8x rsz=%d' % (name, va, vsz, ra, rsz))

    def rva2off(rva):
        for name, va, vsz, ra, rsz in secs:
            if va <= rva < va + max(vsz, rsz):
                return ra + (rva - va)
        return None

    off = rva2off(imp_rva)
    if off is None:
        print('导入表 RVA 无法映射'); return 1

    idx = 0
    total = 0
    while True:
        o = off + idx * 20
        oft, tstamp, fwd, namerva, firstthunk = (u32(b, o), u32(b, o + 4), u32(b, o + 8),
                                                u32(b, o + 12), u32(b, o + 16))
        if namerva == 0 and firstthunk == 0:
            break
        dll = cstr(b, rva2off(namerva))
        # 用 OriginalFirstThunk 读名字
        names = []
        t = rva2off(oft if oft else firstthunk)
        k = 0
        while t is not None:
            ent = o + 0
            thunk = u32(b, t + k * 4) if not pe32plus else struct.unpack_from('<Q', b, t + k * 8)[0]
            if thunk == 0:
                break
            if not (thunk & 0x80000000):
                nm_off = rva2off(thunk)
                if nm_off is not None:
                    names.append(cstr(b, nm_off + 2))
            else:
                names.append('ORD#%d' % (thunk & 0xFFFF))
            k += 1
            if k > 300:
                break
        print('\nDLL %s  (IAT 起始 RVA=%#x)  函数 %d 个:' % (dll, firstthunk, len(names)))
        for i, nm in enumerate(names):
            print('   [%3d] %s' % (i, nm))
        total += len(names)
        idx += 1
        if idx > 20:
            break
    print('\n导入函数 总数 = %d' % total)
    return 0


if __name__ == '__main__':
    sys.exit(main())
