# -*- coding: utf-8 -*-
"""看内核用 __asm 的例子 (tests/kernel 里)"""
import io, sys, os, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
kdir = 'tests/kernel'
for f in sorted(os.listdir(kdir)):
    if f.endswith('.c'):
        t = open(os.path.join(kdir, f), 'rb').read().decode('utf-8', errors='replace')
        if '__asm' in t:
            print('=== %s ===' % f)
            for i, l in enumerate(t.split('\n'), 1):
                if '__asm' in l:
                    print('  %d: %s' % (i, l.strip()[:100]))
