# -*- coding: utf-8 -*-
import subprocess, re, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
for f in ['v1.exe', 'v2.exe']:
    r = subprocess.run(['objdump', '-d', f], capture_output=True, text=True, encoding='utf-8', errors='replace', cwd=r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
    # find lea rax/rcx,[rip+X] where X is in .data (0x4f2000+ for v1)
    hits = []
    for m in re.finditer(r'^\s*([0-9a-f]+):\s+[0-9a-f ]+\s+lea\s+\S+,\s*([0-9a-f]+)\(%rip\)\s+#\s*([0-9a-f]+)', r.stdout, re.M):
        insn_va = int(m.group(1), 16)
        target = int(m.group(3), 16)
        if 0x4f0000 <= target <= 0x600000:  # .data region (before vars)
            hits.append((insn_va, target))
    print(f, 'lea-to-early-data count:', len(hits))
    # print a few with the preceding function context (last 10 lines)
    lines = r.stdout.splitlines()
    for insn_va, target in hits[:8]:
        print('  insn at 0x%x -> 0x%x' % (insn_va, target))
