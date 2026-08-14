# -*- coding: utf-8 -*-
"""gdb: 断 0x413034, 看 out 完整来源 (rbp-0x1940) + 调用上下文"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

gdb_cmd = '''set pagination off
break *0x413034
run srclib_jiayan/qcc_work.jy -o scratch_test/_v2s.exe
printf "=== 0x413034 ===\\n"
printf "rbp=0x%lx rsp=0x%lx\\n", $rbp, $rsp
printf "out(-0x1940)=0x%lx (高32=0x%x 低32=0x%x)\\n", *(unsigned long long*)($rbp-0x1940), *(int*)($rbp-0x193c), *(int*)($rbp-0x1940)
printf "r8(调用者outp)=0x%lx\\n", $r8
x/8gx $rsp
printf "\\n反汇编 out 赋值 (0x410b30-0x410b40):\\n"
x/8i 0x410b30
quit
'''
open('scratch_test/_gdb.txt','w').write(gdb_cmd)
r = subprocess.run(['gdb','-batch','-x','scratch_test/_gdb.txt','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=400)
print('=== 0x413034 完整 ===')
for line in (r.stdout + r.stderr).splitlines()[-30:]:
    print(' ', line)
