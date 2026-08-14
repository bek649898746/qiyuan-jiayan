# -*- coding: utf-8 -*-
data = open('srclib/qcc_x86.c','rb').read()
print('写字节 utf8 at:', data.find('写字节'.encode('utf-8')))
print('写字节 gbk at:', data.find('写字节'.encode('gbk')))
i = data.find(b'asm_emit("    ')
print('first asm_emit text:', repr(data[i:i+40]))
