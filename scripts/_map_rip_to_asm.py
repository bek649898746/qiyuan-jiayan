# -*- coding: utf-8 -*-
"""Map a runtime RIP into the -S asm text to locate source context.
The -S output (.asm.asm) is one text line per emitted instruction, in order.
We reconstruct approximate byte offsets by parsing each line's mnemonic count
is NOT reliable; instead we use the *order*: the host emitted these asm_emit
lines sequentially, so line index ~ instruction sequence index. We instead
map via the .asm (binary) section order: objdump each instruction size.
Simpler approach: locate the last call/jmp target <= RIP to find function, then
print surrounding asm lines around the matching instruction.

Here we use objdump on v1b3.exe to find the function containing RIP.
"""
import subprocess, sys, re

exe = sys.argv[1] if len(sys.argv) > 1 else 'v1b3.exe'
rip = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x410ad0

out = subprocess.run(['objdump', '-d', exe], capture_output=True, text=True, errors='replace').stdout
lines = out.splitlines()
# collect instruction addresses
addr_re = re.compile(r'^\s*([0-9a-f]+):')
insts = []
for i, l in enumerate(lines):
    m = addr_re.match(l)
    if m:
        insts.append((int(m.group(1), 16), i))
# find nearest instruction <= rip
target = None
for a, i in insts:
    if a <= rip:
        target = (a, i)
    else:
        break
if target is None:
    print('no instruction found')
    sys.exit(1)
a, i = target
print(f'RIP {rip:#x} maps to instruction at {a:#x} (line {i})')
# print surrounding 30 lines
for j in range(max(0, i - 25), min(len(lines), i + 8)):
    print(lines[j])
