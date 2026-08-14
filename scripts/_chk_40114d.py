# -*- coding: utf-8 -*-
"""gdb: 断 0x40114d (malloc/realloc?), 看参数"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

# 0x40114d 是 malloc? 反汇编确认
r = subprocess.run(['objdump','-d','--start-address=0x40114d','--stop-address=0x401170','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
print('=== 0x40114d ===')
for l in (r.stdout + r.stderr).splitlines():
    print(' ', l)
