# -*- coding: utf-8 -*-
"""反汇编 fn_macro_expand_to (0x40ff0f-0x410d00), 找 -0x1940 写入"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

r = subprocess.run(['objdump','-d','--start-address=0x40ff0f','--stop-address=0x410d00','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
lines = (r.stdout + r.stderr).splitlines()
print('=== -0x1940 的所有引用 ===')
for l in lines:
    if '1940' in l:
        print(' ', l.strip())
