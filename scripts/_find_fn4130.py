# -*- coding: utf-8 -*-
"""定位 0x413034 函数"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

r = subprocess.run(['objdump','-d','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=180)
lines = (r.stdout + r.stderr).splitlines()
for i, l in enumerate(lines):
    if '413034' in l:
        for j in range(i-150, 0, -1):
            if 'push   %rbp' in lines[j]:
                print('函数入口:', lines[j].strip())
                for k in range(j, j+15):
                    print('  ', lines[k].strip()[:90])
                break
        break
