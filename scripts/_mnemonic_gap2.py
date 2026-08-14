# -*- coding: utf-8 -*-
"""助记符差集 v2: qcc asm_emit vs asm_zh strcmp(mn,...) — GBK 处理."""
import re, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

def read_gbk(path):
    return open(path, 'rb').read().decode('gbk', errors='replace')

# 1. qcc asm_emit 助记符: asm_emit("    <词>...") 第一个词, 去 \n 转义
qcc = read_gbk(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c')
qcc_mn = set()
for m in re.finditer(r'asm_emit\("([^"]*)"', qcc):
    t = m.group(1).replace('\\n', ' ').replace('\\t', ' ').strip()
    if not t or t.startswith(';'):
        continue
    word = re.split(r'[\s,%+;\[\]]', t)[0]
    if word and not re.match(r'^r\d+$', word):
        qcc_mn.add(word)

# 2. asm_zh 助记符: strcmp(mn,"xxx")
asmzh = read_gbk(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\asm_zh.c')
asmzh_mn = set(re.findall(r'strcmp\(mn,"([^"]+)"\)', asmzh))

print(f'qcc asm_emit 助记符: {len(qcc_mn)} | asm_zh strcmp(mn): {len(asmzh_mn)}')
missing = sorted(qcc_mn - asmzh_mn)
print(f'=== asm_zh 缺失: {len(missing)} ===')
for w in missing:
    print(' ', repr(w))
