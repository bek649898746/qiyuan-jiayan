# -*- coding: utf-8 -*-
"""检查镜像 asm_enc_string 是否已同步"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', 'rb').read().decode('utf-8-sig', errors='replace')
print('镜像 asm_enc_string:', 'asm_enc_string' in t)
print('镜像 asm_reg_id:', 'asm_reg_id' in t)
