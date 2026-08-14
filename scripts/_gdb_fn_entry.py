# -*- coding: utf-8 -*-
"""gdb: 断 0x40ff0f 入口, 看 fn_macro_expand_to 参数"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

gdb_cmd = '''set pagination off
break *0x40ff0f
run srclib_jiayan/qcc_work.jy -o scratch_test/_v2s.exe
printf "=== fn_macro_expand_to 入口 ===\\n"
printf "rcx(seg)=0x%lx rdx=0x%lx r8=0x%lx r9=0x%lx\\n", $rcx, $rdx, $r8, $r9
printf "seg 内容: "
x/16bx $rcx
quit
'''
open('scratch_test/_gdb.txt','w').write(gdb_cmd)
r = subprocess.run(['gdb','-batch','-x','scratch_test/_gdb.txt','./v1.exe'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=400)
print('=== 入口参数 ===')
for line in (r.stdout + r.stderr).splitlines()[-20:]:
    print(' ', line)
