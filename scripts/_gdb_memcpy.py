# -*- coding: utf-8 -*-
"""gdb: 断 0x4dfd49 (memcpy), 看参数 (str_row 等)"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

gdb_cmd = '''set pagination off
break *0x4dfd46
run srclib_jiayan/qcc_work.jy -o scratch_test/_v2x.exe
printf "=== 0x4dfd46 (memcpy 循环) ===\\n"
printf "rcx=0x%lx rdx=0x%lx r8=0x%lx r9=0x%lx\\n", $rcx, $rdx, $r8, $r9
printf "r8 内容: "
x/8bx $r8
printf "r9 内容: "
x/8bx $r9
quit
'''
open('scratch_test/_gdb.txt','w').write(gdb_cmd)
r = subprocess.run(['gdb','-batch','-x','scratch_test/_gdb.txt','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=400)
print('=== 0x4dfd46 memcpy ===')
for line in (r.stdout + r.stderr).splitlines()[-25:]:
    print(' ', line)
