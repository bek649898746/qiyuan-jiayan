# -*- coding: utf-8 -*-
"""在文件头 /* 后插入 ASCII 版权行 (CRLF)"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

p = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c'
d = open(p, 'rb').read()

if b'bek649898746' in d:
    print('版权已存在')
else:
    insert = b' * Author: Zheng Yuhe + Qiyuan (seed=828) | GitHub: bek649898746/qiyuan-jiayan\r\n'
    # 在 "/*\r\n" 后插入
    anchor = b'/*\r\n'
    idx = d.find(anchor)
    if idx >= 0:
        new = d[:idx+len(anchor)] + insert + d[idx+len(anchor):]
        open(p, 'wb').write(new)
        print('[OK] 宿主版权已插入 @%d' % idx)
    else:
        print('[FAIL] 未找到 /*\\r\\n')
