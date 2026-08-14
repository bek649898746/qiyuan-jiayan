# -*- coding: utf-8 -*-
d2 = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v2.exe','rb').read()
d3 = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v3.exe','rb').read()
pat = b'#ifdef\x00#ifndef\x00#if\x00#elif\x00#else\x00#endif\x00#undef\x00#define\x00_va_alloc\x00__VA_ARGS__\x00'
i2 = d2.find(pat)
i3 = d3.find(pat)
print('v2 sdat str2-11 at 0x%x, v3 at 0x%x' % (i2, i3))

# Scan backwards from the marker to find the sdat start (str 0).
# sdat strings are packed with NUL separators; scan back until we hit non-string code.
def sdat_start(d, marker):
    # walk back from marker, treating [printable+utf8] bytes as string content
    j = marker - 1
    # skip back over the string immediately before marker (its NUL is at marker-1)
    # We just look at the 400 bytes before and print them
    return max(0, marker - 400)

print('=== v2 before marker (400 bytes) ===')
print(d2[i2-400:i2])
print('=== v3 before marker (400 bytes) ===')
print(d3[i3-400:i3])
