# -*- coding: utf-8 -*-
d2 = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v2.exe','rb').read()
d3 = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v3.exe','rb').read()
pat = b'#ifdef\x00#ifndef\x00#if\x00#elif\x00#else\x00#endif\x00#undef\x00#define\x00_va_alloc\x00__VA_ARGS__\x00'
i2 = d2.find(pat)
i3 = d3.find(pat)
print('v2 sdat marker at 0x%x, v3 at 0x%x' % (i2, i3))

# compare from marker forward — sdat content should be identical if layout matches
MAX = 400000
seg2 = d2[i2:i2+MAX]
seg3 = d3[i3:i3+MAX]
n = min(len(seg2), len(seg3))
same = 0
while same < n and seg2[same] == seg3[same]:
    same += 1
print('common bytes after marker:', same)
if same < n:
    print('first diff at +%d:' % same)
    print('  v2:', d2[i2+same-30:i2+same+30])
    print('  v3:', d3[i3+same-30:i3+same+30])
else:
    print('sdat content identical for', n, 'bytes')
