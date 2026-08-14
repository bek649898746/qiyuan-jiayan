# -*- coding: utf-8 -*-
"""gdb: 断 realloc (0x4ed010 IAT), 看参数和返回值"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

gdb_cmd = '''set pagination off
break *0x410a55
run srclib_jiayan/qcc_work.jy -o scratch_test/_v2s.exe
printf "=== call *0x4ed010 前 ===\\n"
printf "rcx=0x%lx rdx=0x%lx r8=0x%lx\\n", $rcx, $rdx, $r8
printf "rcx(out) 指向: "
x/8bx $rcx
printf "rdx(cap) 指向: "
x/2gx $rdx
quit
'''
open('scratch_test/_gdb.txt','w').write(gdb_cmd)
r = subprocess.run(['gdb','-batch','-x','scratch_test/_gdb.txt','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=400)
print('=== realloc 调用前 ===')
for line in (r.stdout + r.stderr).splitlines()[-20:]:
    print(' ', line)
