# -*- coding: utf-8 -*-
"""gdb: watch 0x4ed000 (out 全局), 抓写入者"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

# 先确认 0x4ed000 是数据地址 (v1 运行时可访问)
gdb_cmd = '''set pagination off
break *0x40ff0f
run srclib_jiayan/qcc_work.jy -o scratch_test/_v2s.exe
watch *(unsigned long long*)0x4ed000
continue
printf "W1: 0x4ed000=0x%lx rip=0x%lx\\n", *(unsigned long long*)0x4ed000, $rip
x/5i $rip-8
continue
printf "W2: 0x4ed000=0x%lx rip=0x%lx\\n", *(unsigned long long*)0x4ed000, $rip
x/5i $rip-8
continue
printf "W3: 0x4ed000=0x%lx rip=0x%lx\\n", *(unsigned long long*)0x4ed000, $rip
x/5i $rip-8
quit
'''
open('scratch_test/_gdb.txt','w').write(gdb_cmd)
r = subprocess.run(['gdb','-batch','-x','scratch_test/_gdb.txt','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=400)
print('=== watch 0x4ed000 ===')
for line in (r.stdout + r.stderr).splitlines()[-30:]:
    print(' ', line)
