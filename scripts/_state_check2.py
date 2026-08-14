# -*- coding: utf-8 -*-
"""确认宿主/镜像当前状态"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

h = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', 'rb').read()
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', 'rb').read()

print('宿主 asm_enc_string:', b'asm_enc_string' in h)
print('宿主 asm_reg_id:', b'asm_reg_id' in h)
print('宿主 __asm STR 分派:', b'nt[c] == STR' in h)
print('宿主 str_tbl:', b'str_tbl[1024][2048]' in h, '(1024)' if b'str_tbl[1024][2048]' in h else '')
print('宿主版权:', b'bek649898746' in h)
print()
print('镜像 asm_enc_string:', b'asm_enc_string' in m)
print('镜像 asm_reg_id:', b'asm_reg_id' in m)
print('镜像 regn 平铺表:', b'regn[40][8]' in m)
print('镜像 __asm STR 分派:', 'asm_enc_string(s)' in m.decode('utf-8-sig', errors='replace'))
print('镜像 str_tbl:', b'str_tbl[1024][2048]' in m)
print('镜像 strtol/atoi:', b'strtol' in m, b'atoi' in m)
