# -*- coding: utf-8 -*-
"""gdb: v1 编 v2 崩溃 (镜像编码器)"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

gdb_cmd = '''set pagination off
run srclib_jiayan/qcc_work.jy -o scratch_test/_v2x.exe
printf "=== 崩溃 ===\\n"
bt 6
info registers rip rax rbx rcx rdx rsi rdi r8 r9
x/4i $rip-8
quit
'''
open('scratch_test/_gdb.txt','w').write(gdb_cmd)
r = subprocess.run(['gdb','-batch','-x','scratch_test/_gdb.txt','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=400)
print('=== 崩溃 ===')
for line in (r.stdout + r.stderr).splitlines()[-25:]:
    print(' ', line)
