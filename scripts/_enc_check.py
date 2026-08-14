# -*- coding: utf-8 -*-
"""对比 .段 字节编码."""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

asm = open(r'scratch_test/_h2t.asm.asm', 'rb').read()
for line in asm.split(b'\n'):
    if b'data_vsize' in line:
        print('.asm .段 行:', repr(line[:50]))
        # 提取 .段 部分的字节
        m = line.find(b'data_vsize')
        seg = line[max(0, m-6):m]
        print('  .段 字节:', seg.hex(), '->', seg)
        break

c = open(r'srclib/asm_zh.c', 'rb').read()
j = c.find(b'data_vsize=0x')
seg = c[j-40:j]
print('asm_zh.c 分支前:', seg.hex())
# 找 .段 字符串字面量
for m in range(j-40, j):
    if c[m:m+1] == b'"':
        print('  .段 字面量字节:', c[m:m+20].hex(), '->', c[m:m+20])
        break
