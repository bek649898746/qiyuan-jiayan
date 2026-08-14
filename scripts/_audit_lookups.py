# -*- coding: utf-8 -*-
import re
# audit all lookups that iterate vars[] and check parse_base guard
for f in [r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c']:
    src = open(f, 'rb').read().decode('utf-8', errors='replace')
    lines = src.split(chr(10))
    print('=== host qcc_x86.c lookups without explicit parse_base ===')
    for i, l in enumerate(lines):
        if 'vs_n() - 1; i >= 0' in l or 'vs_n() - 1; i>=0' in l:
            # check if var_codegen_visible is in the loop (internal guard)
            guard = 'var_codegen_visible' in lines[i+1] if i+1 < len(lines) else False
            # look ahead a few lines for the loop body
            for k in range(i, min(i+4, len(lines))):
                if 'var_codegen_visible' in lines[k]:
                    guard = True
                    break
            print('  line %d: %s [var_codegen_visible=%s]' % (i+1, l.strip()[:100], guard))
