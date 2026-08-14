# -*- coding: utf-8 -*-
"""str_tbl 1024→2048 (宿主+镜像) — Phase1 L2 字符串表扩容"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

for path, is_mirror in [(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', False),
                        (r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', True)]:
    t = open(path, 'rb').read().decode('utf-8-sig' if is_mirror else 'utf-8', errors='replace')
    n = t.count('str_tbl[1024][2048]')
    t = t.replace('str_tbl[1024][2048]', 'str_tbl[2048][2048]')
    open(path, 'wb').write(('\ufeff' + t).encode('utf-8') if is_mirror else t.encode('utf-8'))
    print('%s: str_tbl 1024->2048 (替换 %d 处)' % ('镜像' if is_mirror else '宿主', n))
