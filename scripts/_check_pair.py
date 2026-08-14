# -*- coding: utf-8 -*-
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
src = open(r'C:\Users\Administrator\Desktop\git-2.45.2\_fn_out.txt', encoding='utf-8', errors='replace').read()
# 找 switch 使用点 (@154474) 前的字符串/注释状态
for target in [12024, 154474]:
    seg = src[max(0, target-5000):target]
    in_str = False; in_char = False; in_cmt = False; bad = None
    i = 0
    while i < len(seg):
        c = seg[i]
        if in_str:
            if c == '\\':
                i += 2; continue
            if c == '"':
                in_str = False
        elif in_char:
            if c == '\\':
                i += 2; continue
            if c == "'":
                in_char = False
        elif in_cmt:
            if c == '*' and i+1 < len(seg) and seg[i+1] == '/':
                in_cmt = False; i += 2; continue
        else:
            if c == '"': in_str = True
            elif c == "'": in_char = True
            elif c == '/' and i+1 < len(seg) and seg[i+1] == '*': in_cmt = True; i += 2; continue
        i += 1
    print(f'@ {target}: in_str={in_str} in_char={in_char} in_cmt={in_cmt}')
