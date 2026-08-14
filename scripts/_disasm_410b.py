# -*- coding: utf-8 -*-
"""反汇编 0x410b00-0x410b40 (out 赋值)"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
r = subprocess.run(['objdump','-d','--start-address=0x410b00','--stop-address=0x410b40','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
for l in (r.stdout + r.stderr).splitlines():
    print(' ', l)
