# -*- coding: utf-8 -*-
"""看镜像 asm_enc_string 完整度"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', 'rb').read().decode('utf-8-sig', errors='replace')
# 找 asm_enc_string 的完整定义
i = t.find('asm_enc_string')
print('asm_enc_string @', i)
if i > 0:
    # 往前找函数签名 (静 空)
    j = t.rfind('静 空', 0, i)
    seg = t[j:i+3000]
    print(seg[:3000])
