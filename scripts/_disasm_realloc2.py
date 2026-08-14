# -*- coding: utf-8 -*-
"""反汇编 v1 (str_tbl 2048) 的 realloc 调用, 看返回值 32/64 位"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

# 崩溃在 fn_macro_expand_to (0x40ff0f), out 更新在 0x410b37
# realloc 调用应该在崩溃前. 找 0x410b37 前的 call
r = subprocess.run(['objdump','-d','--start-address=0x410a00','--stop-address=0x410b40','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
print('=== v1 0x410a00-0x410b40 (realloc + out 更新) ===')
for l in (r.stdout + r.stderr).splitlines():
    print(' ', l)
