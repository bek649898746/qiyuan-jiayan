# -*- coding: utf-8 -*-
"""回滚宿主+镜像到 HEAD (3013a44c 干净基线)"""
import io, sys, subprocess, os
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

r = subprocess.run(['git','checkout','--','srclib/qcc_x86.c','srclib_jiayan/qcc_work.jy'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace')
print('回滚:', r.stdout, r.stderr)
r = subprocess.run(['git','status','--short'], capture_output=True, text=True, encoding='utf-8', errors='replace')
print(r.stdout if r.stdout.strip() else '(clean)')
