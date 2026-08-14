# -*- coding: utf-8 -*-
"""找 v1 里调用 0x40ff0f (fn_macro_expand_to) 的位置"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

r = subprocess.run(['objdump','-d','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=180)
lines = (r.stdout + r.stderr).splitlines()
for i, l in enumerate(lines):
    if 'call' in l and '40ff0f' in l:
        print('  %d: %s' % (i, l.strip()[:80]))
        # 打印上下文
        for k in range(max(0,i-12), i+2):
            print('    %d: %s' % (k, lines[k].strip()[:80]))
        break
