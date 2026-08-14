# -*- coding: utf-8 -*-
"""定位 0x401069 函数"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
r = subprocess.run(['objdump','-d','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=180)
lines = (r.stdout + r.stderr).splitlines()
for i, l in enumerate(lines):
    if '401069' in l:
        for j in range(i-150, 0, -1):
            if 'push   %rbp' in lines[j]:
                print('函数入口:', lines[j].strip())
                # 打印函数开头 + 0x401069 附近
                for k in range(j, j+8):
                    print('  ', lines[k].strip()[:90])
                print('  ...')
                for k in range(i-5, i+5):
                    print('  ', lines[k].strip()[:90])
                break
        break
