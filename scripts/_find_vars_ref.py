# -*- coding: utf-8 -*-
import subprocess, re, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
for f in ['v1.exe', 'v2.exe']:
    r = subprocess.run(['objdump', '-d', f], capture_output=True, text=True, encoding='utf-8', errors='replace', cwd=r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
    hits = []
    for m in re.finditer(r'^\s*([0-9a-f]+):\s+[0-9a-f ]+\s+lea\s+\S+,\s*([0-9a-f]+)\(%rip\)\s+#\s*([0-9a-f]+)', r.stdout, re.M):
        insn_va = int(m.group(1), 16)
        target = int(m.group(3), 16)
        if 0xBE0000 <= target <= 0xC10000:
            hits.append((insn_va, target))
    print(f, 'lea to 0xBE-0xC1 region:', len(hits))
    for h in hits[:10]:
        print('  0x%x -> 0x%x' % h)
    # also find mov [rip+...], imm / mov reg, [rip+...] to vars
    hits2 = []
    for m in re.finditer(r'^\s*([0-9a-f]+):\s+[0-9a-f ]+\s+(?:mov|add|sub|cmp)\s+\S+,\s*([0-9a-f]+)\(%rip\)\s+#\s*([0-9a-f]+)', r.stdout, re.M):
        insn_va = int(m.group(1), 16)
        target = int(m.group(3), 16)
        if 0xBE0000 <= target <= 0xC10000:
            hits2.append((insn_va, target))
    print(f, 'mov etc to 0xBE-0xC1 region:', len(hits2))
    for h in hits2[:10]:
        print('  0x%x -> 0x%x' % h)
