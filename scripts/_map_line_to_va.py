# -*- coding: utf-8 -*-
"""Map a .asm.asm text line to a VA in the corresponding exe.
The .asm.asm has one line per asm_emit'd instruction, in order.
We approximate: the exe's .text starts at 0x401000; each -S line that emits
machine code has a length we can't easily get from text alone.  So instead,
we compute via objdump of the exe: count instructions up to a target.

Simpler: find the exe instruction index matching the -S line index.
"""
import subprocess, sys, re

exe = sys.argv[1] if len(sys.argv) > 1 else 'v1b3.exe'
target_line = int(sys.argv[2]) if len(sys.argv) > 2 else 101918

# objdump the exe, collect instruction addresses in order
out = subprocess.run(['objdump', '-d', exe], capture_output=True, text=True, errors='replace').stdout
lines = out.splitlines()
addr_re = re.compile(r'^\s*([0-9a-f]+):')
addrs = []
for l in lines:
    m = addr_re.match(l)
    if m:
        addrs.append(int(m.group(1), 16))

# -S line index maps to instruction index (with a bias because .asm.asm has
# label/comment lines).  We can't map exactly; but the prim function region
# can be located by searching for a nearby known label.
print(f'objdump instruction count: {len(addrs)}')
print(f'target -S line: {target_line}')
if 0 < target_line < len(addrs):
    print(f'approx VA: 0x{addrs[target_line]:x}')
