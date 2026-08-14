# -*- coding: utf-8 -*-
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
r = subprocess.run(['gcc', '-O2', 'srclib/qcc_x86.c', '-o', 'qcc_x86.exe'], capture_output=True, text=True, encoding='utf-8', errors='replace')
print('rc=%d' % r.returncode)
print(r.stderr[-300:] if r.stderr else 'OK')
