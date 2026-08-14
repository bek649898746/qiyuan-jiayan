# -*- coding: utf-8 -*-
import re
h = open('srclib/qcc_x86.c','rb').read().decode('utf-8', errors='replace')
hl = h.splitlines()
for i, l in enumerate(hl, 1):
    if 'static int nll[' in l or 'nll[ASZ]' in l or 'int nll[' in l:
        print(i, l.strip()[:100])
        for j in range(max(0,i-3), min(len(hl), i+4)):
            print(' ', j+1, hl[j].strip()[:100])
