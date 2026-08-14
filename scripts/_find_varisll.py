# -*- coding: utf-8 -*-
import subprocess, re, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
# find var_is_ll: short function that iterates vars array and returns is_ll field
# Look for the pattern: lea rax,[rip+disp] where disp points into .data (vars region)
# and the function does strcmp + return
for f in ['v1.exe', 'v2.exe']:
    r = subprocess.run(['objdump', '-d', f], capture_output=True, text=True, encoding='utf-8', errors='replace', cwd=r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
    # find functions: look for lea rax,[rip+X] with X large (>0x400000), in a small function
    # We search for var_is_ll by its structure: compares vars[i].name (32-byte) with arg
    # Pattern: imul or lea with stride ~104-112
    lines = r.stdout.splitlines()
    for i, l in enumerate(lines):
        if 'lea' in l and '0x' in l and ('%rip' in l or '(rip)' in l):
            pass
    # Find all "mov eax, [reg + reg*4 + NNN]" where NNN is the is_ll offset (~92-100)
    hits = []
    for m in re.finditer(r'(0x[0-9a-f]+):\s+[0-9a-f ]*8b\s+\S+,\s*0x([0-9a-f]+)\((.*?)\)', r.stdout):
        va = int(m.group(1), 16)
        off = int(m.group(2), 16)
        if 80 <= off <= 120:  # is_ll offset range in vars struct
            hits.append((va, off, m.group(3)))
    print(f, 'field access offsets 80-120:', hits[:15])
