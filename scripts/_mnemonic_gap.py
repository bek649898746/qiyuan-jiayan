# -*- coding: utf-8 -*-
"""助记符差集: qcc asm_emit 输出 vs asm_zh 支持 — 找 asm_zh 覆盖缺口."""
import re, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

# 1. qcc asm_emit 的助记符 (asm_emit("  <助记符> ...") 或 asm_emit("    <助记符>...")
qcc_src = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', encoding='utf-8', errors='replace').read()
qcc_mn = set()
for m in re.finditer(r'asm_emit\("([^"]*)"', qcc_src):
    txt = m.group(1)
    # 提取助记符: 去掉前导空格, 取第一个词 (中文/英文助记符)
    t = txt.strip()
    if not t:
        continue
    # 助记符 = 第一个 token (中文词或英文词), 跳过标签/注释
    if t.startswith(';') or t.startswith('_') or t.startswith('.'):
        continue
    word = re.split(r'[\s,%+;\[\]]', t)[0]
    if word and not word.startswith('r') and not word.startswith('['):
        qcc_mn.add(word)

# 2. asm_zh 支持的助记符 (strcmp 链或 mnemonic 匹配)
asmzh_src = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\asm_zh.c', encoding='utf-8', errors='replace').read()
asmzh_mn = set()
# asm_zh 用 strcmp(opc, "xxx") 形式? 或数组? 找所有引号字符串
for m in re.finditer(r'"([^"]+)"', asmzh_src):
    w = m.group(1)
    # 只收中文助记符 (可能) — asm_zh 的助记符表
    if re.match(r'^[\u4e00-\u9fff]{2,}$', w) or (re.match(r'^[a-z]+$', w) and len(w) >= 2 and w not in ('if', 'int', 'for', 'char', 'void', 'return', 'static', 'const', 'unsigned', 'struct')):
        asmzh_mn.add(w)

print(f'qcc asm_emit 助记符: {len(qcc_mn)}')
print(f'asm_zh 助记符: {len(asmzh_mn)}')
print()
missing = sorted(qcc_mn - asmzh_mn)
print(f'=== asm_zh 缺失 (qcc 有, asm_zh 无): {len(missing)} ===')
for w in missing:
    print(' ', w)
print()
extra = sorted(asmzh_mn - qcc_mn)
print(f'=== asm_zh 额外 (asm_zh 有, qcc 无): {len(extra)} ===')
print(' ', extra)
