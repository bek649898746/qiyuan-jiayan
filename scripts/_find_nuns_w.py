# -*- coding: utf-8 -*-
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
for name in ['nuns[', 'ndbl[', 'nll_hi[', 'nuns]', 'ndbl]']:
    print('===', name, '===')
    for i, l in enumerate(lines):
        if name in l:
            print((i+1), '|', l.strip()[:130])
