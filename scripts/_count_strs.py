# -*- coding: utf-8 -*-
"""数镜像源码的字符串字面量数量"""
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', 'rb').read().decode('utf-8-sig', errors='replace')
# 数 "..." 字符串字面量 (粗略)
strs = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', t)
print('字符串字面量数:', len(strs))
# 数唯一的
print('唯一:', len(set(strs)))
