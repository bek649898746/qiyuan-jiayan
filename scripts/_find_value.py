# -*- coding: utf-8 -*-
d2 = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v2.exe','rb').read()
d3 = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v3.exe','rb').read()
for f, d in [('v2', d2), ('v3', d3)]:
    print('===', f)
    start = 0
    cnt = 0
    while True:
        i = d.find(b'value\x00', start)
        if i < 0 or cnt > 20:
            break
        # show what follows
        print('  value at 0x%x, next 40:' % i, d[i:i+45])
        start = i + 1
        cnt += 1
