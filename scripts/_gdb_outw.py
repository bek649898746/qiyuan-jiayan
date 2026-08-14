# -*- coding: utf-8 -*-
"""gdb: 断 0x413034 (写 out), 看 out 基址来源"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

gdb_cmd = '''set pagination off
break *0x413034
run srclib_jiayan/qcc_work.jy -o scratch_test/_v2s.exe
printf "=== 写 out ===\\n"
printf "out(-0x1940)=0x%lx idx(-0x192c)=0x%lx 写地址=0x%lx\\n", *(unsigned long long*)($rbp-0x1940), *(int*)($rbp-0x192c), *(unsigned long long*)($rbp-0x1940)+*(int*)($rbp-0x192c)
printf "rbp=0x%lx\\n", $rbp
x/6i $rip-16
quit
'''
open('scratch_test/_gdb.txt','w').write(gdb_cmd)
r = subprocess.run(['gdb','-batch','-x','scratch_test/_gdb.txt','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=400)
print('=== 写 out 现场 ===')
for line in (r.stdout + r.stderr).splitlines()[-20:]:
    print(' ', line)
