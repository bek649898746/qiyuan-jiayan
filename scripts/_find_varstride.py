# -*- coding: utf-8 -*-
import subprocess, re, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
# disassemble and find imul patterns that could be vars stride
for f in ['v1.exe', 'v2.exe']:
    r = subprocess.run(['objdump', '-d', f], capture_output=True, text=True, encoding='utf-8', errors='replace', cwd=r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
    # find imul reg,reg,N patterns near lea [rip+...] (vars access)
    imuls = []
    for m in re.finditer(r'(\w+):\s+[0-9a-f ]+imul\s+\$0x([0-9a-f]+),%[er](\w+),%[er](\w+)', r.stdout):
        va = int(m.group(1), 16)
        imm = int(m.group(2), 16)
        imuls.append((va, imm))
    # common imul immediates
    from collections import Counter
    c = Counter(imm for _, imm in imuls)
    print(f, 'imul imm counts:', c.most_common(12))
