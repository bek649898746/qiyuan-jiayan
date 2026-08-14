# -*- coding: utf-8 -*-
"""对比宿主/镜像 STACK_PAD_SIZE"""
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
for path, label in [(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', '宿主'),
                    (r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', '镜像')]:
    d = open(path, 'rb').read()
    m = re.search(rb'STACK_PAD_SIZE\s+(\w+)', d)
    print('%s: STACK_PAD_SIZE = %s' % (label, m.group(1).decode() if m else '?'))
    # 也找 CODE_BUF_CAP
    m2 = re.search(rb'CODE_BUF_CAP\s+(\w+)', d)
    print('  CODE_BUF_CAP = %s' % (m2.group(1).decode() if m2 else '?'))
