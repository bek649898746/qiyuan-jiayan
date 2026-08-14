# -*- coding: utf-8 -*-
"""回滚 str_tbl 2048 (宿主+镜像+守卫)"""
import io, sys, subprocess, os
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

# 宿主: str_tbl 2048->1024, 守卫 2048->1024, 版权保留
p = 'srclib/qcc_x86.c'
d = open(p, 'rb').read()
d = d.replace(b'static char str_tbl[2048][2048]', b'static char str_tbl[1024][2048]')
d = d.replace(b'if (str_cnt >= 2048)', b'if (str_cnt >= 1024)')
open(p, 'wb').write(d)
print('宿主回滚 (str_tbl 1024)')

# 镜像: str_tbl 2048->1024, 守卫 2048->1024
p2 = 'srclib_jiayan/qcc_work.jy'
d2 = open(p2, 'rb').read()
d2 = d2.replace('静 字 str_tbl[2048][2048]'.encode('utf-8'), '静 字 str_tbl[1024][2048]'.encode('utf-8'))
d2 = d2.replace('str_cnt >= 2048'.encode('utf-8'), 'str_cnt >= 1024'.encode('utf-8'))
open(p2, 'wb').write(d2)
print('镜像回滚 (str_tbl 1024)')

# 验证
r = subprocess.run(['git','status','--short'], capture_output=True, text=True, encoding='utf-8', errors='replace')
print(r.stdout if r.stdout.strip() else '(clean)')
