# -*- coding: utf-8 -*-
d2 = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v2.exe','rb').read()
d3 = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v3.exe','rb').read()
pat = b'value\x00\x27) { i++; '
i2 = d2.find(pat)
i3 = d3.find(pat)
print('v2 sdat start: 0x%x  v3 sdat start: 0x%x' % (i2, i3))
if i2 >= 0 and i3 >= 0:
    seg2 = d2[i2:i2+200000]
    seg3 = d3[i3:i3+200000]
    print('sdat first 200000 bytes equal:', seg2 == seg3)
    if seg2 != seg3:
        n = min(len(seg2), len(seg3))
        for k in range(n):
            if seg2[k] != seg3[k]:
                print('first sdat diff at off %d (v2 0x%x v3 0x%x)' % (k, i2+k, i3+k))
                print('v2:', d2[i2+k-24:i2+k+40])
                print('v3:', d3[i3+k-24:i3+k+40])
                break
    # find end of common region
    n = min(len(seg2), len(seg3))
    same = 0
    while same < n and seg2[same] == seg3[same]:
        same += 1
    print('common sdat length: %d (0x%x)' % (same, same))
