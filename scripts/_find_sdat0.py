# -*- coding: utf-8 -*-
d2 = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v2.exe','rb').read()
d3 = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v3.exe','rb').read()

def show(d, a, b):
    s = d[a:b]
    out = []
    for c in s:
        if 32 <= c < 127:
            out.append(chr(c))
        else:
            out.append('.')
    return ''.join(out)

for f, d in [('v2', d2), ('v3', d3)]:
    # find the sdat segment: search for "#ifdef\x00#ifndef\x00#if\x00#elif" (strings 2-5)
    pat = b'#ifdef\x00#ifndef\x00#if\x00#elif\x00#else\x00#endif\x00#undef\x00#define\x00_va_alloc\x00__VA_ARGS__\x00'
    i = d.find(pat)
    print(f, 'sdat marker (str 2-11) at 0x%x' % i)
    # dump 260 bytes before the #ifdef (should contain str0 'value' and str1 code snippet)
    print('  before:', show(d, i-260, i))
