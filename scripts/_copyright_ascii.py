# -*- coding: utf-8 -*-
"""在宿主文件头插入 ASCII 版权声明 (字节安全, 不碰 GBK 字节)
在 "/*" 后插入一行 ASCII 版权"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

p = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c'
d = open(p, 'rb').read()

# 在文件开头的 "/*\n" 后插入版权行 (ASCII)
header_ascii = b'/*\r\n * Author: Zheng Yuhe + Qiyuan (seed=828) | GitHub: bek649898746/qiyuan-jiayan\r\n'
# 检查是否已有
if b'bek649898746' in d:
    print('版权已存在')
else:
    # 在开头 "/*" 后插入
    if d.startswith(b'/*'):
        idx = d.find(b'/*')
        # 在 "/*" 后插入版权 (保留原注释)
        new = d[:idx+2] + b'\r\n' + header_ascii[3:] + d[idx+2:]
        open(p, 'wb').write(new)
        print('[OK] 宿主版权已插入 (ASCII)')
    else:
        print('[FAIL] 文件头不是 /* 开头')
