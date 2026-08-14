# -*- coding: utf-8 -*-
"""干净基线 + str_tbl 2048 (宿主+镜像+守卫) — 字节安全"""
import io, sys, os
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

# 宿主: str_tbl 1024->2048, 守卫 1024->2048 (字节模式, ASCII 替换)
p = 'srclib/qcc_x86.c'
d = open(p, 'rb').read()
n1 = d.count(b'static char str_tbl[1024][2048]')
d = d.replace(b'static char str_tbl[1024][2048]', b'static char str_tbl[2048][2048]')
n2 = d.count(b'if (str_cnt >= 1024)')
d = d.replace(b'if (str_cnt >= 1024)', b'if (str_cnt >= 2048)')
open(p, 'wb').write(d)
print('宿主: str_tbl %d 处, 守卫 %d 处' % (n1, n2))

# 镜像: str_tbl 1024->2048, 守卫 1024->2048
p2 = 'srclib_jiayan/qcc_work.jy'
d2 = open(p2, 'rb').read()
m1 = d2.count('静 字 str_tbl[1024][2048]'.encode('utf-8'))
d2 = d2.replace('静 字 str_tbl[1024][2048]'.encode('utf-8'), '静 字 str_tbl[2048][2048]'.encode('utf-8'))
m2 = d2.count('str_cnt >= 1024'.encode('utf-8'))
d2 = d2.replace('str_cnt >= 1024'.encode('utf-8'), 'str_cnt >= 2048'.encode('utf-8'))
open(p2, 'wb').write(d2)
print('镜像: str_tbl %d 处, 守卫 %d 处' % (m1, m2))
