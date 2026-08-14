# -*- coding: utf-8 -*-
"""gdb: 断 0x410b12, 看 *outp 读 (32 vs 64 位)"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

# 反汇编 0x410b12 确认是 32 位 mov eax
r = subprocess.run(['objdump','-d','--start-address=0x410b0f','--stop-address=0x410b20','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
print('=== 0x410b0f-0x410b20 (*outp 读) ===')
for l in (r.stdout + r.stderr).splitlines():
    print(' ', l)
