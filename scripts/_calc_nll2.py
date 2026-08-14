# -*- coding: utf-8 -*-
"""Compute .data slot indices of the mirror's statics by tracing file-scope
registrations in source order. Mirrors how var_static_arr/struct compute slots.
"""
import re

raw = open(r'srclib_jiayan\qcc_work.jy', 'rb').read()
txt = raw.decode('utf-8')
lines = txt.splitlines()

STACK_PAD = None
for l in lines:
    m = re.match(r'#define\s+STACK_PAD_SIZE\s+(\d+)', l)
    if m:
        STACK_PAD = int(m.group(1))
print('STACK_PAD_SIZE =', STACK_PAD)

# The slots are assigned during file-scope parse. We trace the static decls.
# Registration happens through var_static / var_static_arr / var_static_struct.
# The PARSER processes `静 <type> <name>[dims];` at file scope.
# We approximate by scanning top-level `静` lines and computing slots like var_static_arr:
#   slots = count; if (esz > 4) slots = count*esz/4   (char esz=1, int/double esz=4/8, rows=2nd dim)
# For vars (struct), slots = count * struct_size/4.

def dim_expr(s):
    s = s.strip()
    if s.isdigit():
        return int(s)
    return None

slots = 0
log = []
# scan until we hit the first function definition (静 整 name(...) {  or 整 name(...) {)
fn_starts = [i for i, l in enumerate(lines) if re.match(r'^\s*静\s+整\s+[A-Za-z_]\w*\s*\(', l)]
first_fn = fn_starts[0] if fn_starts else len(lines)

i = 0
while i < len(lines) and i < first_fn:
    l = lines[i]
    m = re.match(r'^\s*静\s+((?:无符号\s+)?(?:整|字|浮|长|构))\s+(\*?)([A-Za-z_]\w*)\s*(.*)$', l)
    if m:
        typ = m.group(1)
        name = m.group(3)
        rest = m.group(4)
        is_char = '字' in typ
        is_dbl = '浮' in typ
        if name == '__pad0':
            slots += STACK_PAD
            log.append((name, STACK_PAD, slots))
            i += 1
            continue
        if 'struct' in typ or '构' in typ:
            # struct array: struct {..} name[N]  — complex; vars[] = 4096 entries
            # compute struct size from the inline def if present on this/next lines
            sz = None
            if 'vars' in name:
                sz = 120  # vars struct
            elif 'stypes' in name:
                sz = 64
            elif 'macros' in name:
                sz = 64
            elif 'func_tbl' in name:
                sz = 48
            elif 'crel' in name:
                sz = 24
            elif 'csym' in name:
                sz = 76
            elif 'dbl_patches' in name or 'str_patches' in name or 'fn_patches' in name:
                sz = 8
            elif 'label_pos' in name:
                sz = 4
            if sz:
                cnt = 1
                m2 = re.search(r'\[(\d+)\]', l)
                if m2:
                    cnt = int(m2.group(1))
                # count = 1 => struct var; >1 array
                nslots = (sz * cnt + 3) // 4 if cnt > 1 else (sz + 3) // 4
                slots += nslots
                log.append((name, nslots, slots))
            i += 1
            continue
        # scalar or array of int/char/double
        dims = re.findall(r'\[([^\]]+)\]', rest)
        cnt = 1
        esz = 4
        if is_char:
            esz = 1
        elif is_dbl:
            esz = 8
        for d in dims:
            dv = dim_expr(d)
            if dv is not None:
                cnt *= dv
        # 2D char array: rows from 2nd dim... count = first dim, esz = row size
        if len(dims) == 2:
            cnt = dim_expr(dims[0]) or 1
            row = dim_expr(dims[1]) or 1
            esz = row * esz
        if cnt == 0:
            cnt = 1
        if esz > 4:
            nslots = (cnt * esz + 3) // 4
        else:
            nslots = cnt
        slots += nslots
        log.append((name, nslots, slots))
    i += 1

print('first_fn line:', first_fn)
print('total slots to first fn:', slots)
# find nll/ndbl etc. — they're declared AFTER many functions, we must continue scanning to their decl lines
# nll is at line ~1261. Continue scanning top-level decls past functions is hard; instead find the exact decl lines and
# accumulate slots for decls between. Print slot at the nll decl by scanning to it (functions don't consume slots).
targets = ['ndbl', 'nll', 'nuns', 'pesz', 'str_tbl']
for name in targets:
    for j, l in enumerate(lines):
        if re.search(r'^\s*静\s+(?:无符号\s+)?(?:整|字)\s+%s\s*\[' % name, l):
            print('decl line for', name, ':', j+1)
            break
