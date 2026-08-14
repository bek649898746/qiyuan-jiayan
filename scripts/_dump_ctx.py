# -*- coding: utf-8 -*-
d = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v2.exe','rb').read()
i = d.find(b'[ERR] OOM realloc')
print('OOM realloc at 0x%x' % i)
# dump 600 bytes before as printable
def show(d, a, b):
    s = d[a:b]
    out = []
    for c in s:
        if 32 <= c < 127:
            out.append(chr(c))
        elif c in (0x0a, 0x0d, 0x09):
            out.append('\\n' if c == 0x0a else ('\\r' if c == 0x0d else '\\t'))
        else:
            out.append('.')
    return ''.join(out)
print('--- before OOM realloc (600 bytes) ---')
print(show(d, i-600, i))
print('--- after OOM realloc (200 bytes) ---')
print(show(d, i, i+200))
