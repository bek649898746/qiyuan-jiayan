# -*- coding: utf-8 -*-
"""查 str_tbl 容量 (宿主+镜像)"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
for path, label in [(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', '宿主'),
                    (r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', '镜像')]:
    t = open(path, 'rb').read().decode('utf-8-sig' if 'jiayan' in path else 'utf-8', errors='replace')
    import re
    m = re.search(r'str_tbl\[(\d+)\]\[(\d+)\]', t)
    print('%s: %s' % (label, m.group(0) if m else '?'))
