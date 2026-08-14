# -*- coding: utf-8 -*-
"""gdb 抓 str_tbl 2048 崩溃"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

gdb_cmd = '''set pagination off
run srclib_jiayan/qcc_work.jy -o scratch_test/_v2x.exe
printf "=== 崩溃 ===\\n"
bt 10
info registers rip rsp rbp
quit
'''
open('scratch_test/_gdb.txt','w').write(gdb_cmd)
r = subprocess.run(['gdb','-batch','-x','scratch_test/_gdb.txt','./qcc_x86.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=400)
print('=== 崩溃 ===')
for line in (r.stdout + r.stderr).splitlines()[-30:]:
    print(' ', line)
