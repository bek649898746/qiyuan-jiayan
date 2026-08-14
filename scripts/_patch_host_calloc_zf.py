# -*- coding: utf-8 -*-
"""HOST-ONLY: add zero-fill loop to calloc codegen branch.
Mirror source stays UNCHANGED. Host is gcc-compiled so its own layout is stable.
The generated v1 will have zeroed calloc -> tll/tuns/ddata clean -> maybe converge.
"""
p = 'srclib/qcc_x86.c'
data = open(p, 'rb').read()
NL = b'\r\n'

# Anchor: the calloc branch tail (the FIRST '/* return old counter */' + mov_rr(0,8) + malloc)
anchor = b"""                /* return old counter */""" + NL + b"""                mov_rr(0, 8);""" + NL + b"""            } else if (!strcmp(fname, "malloc")) {"""

print('anchor count:', data.count(anchor))
if data.count(anchor) != 1:
    print('[FAIL]')
    raise SystemExit(1)

# GBK-encoded mnemonics (host asm_emit Chinese are UTF-8 bytes per earlier finding!
# BUT the codegen strings in host are UTF-8-encoded Chinese (verified: '移动 r%d' UTF-8 found).
# So we use UTF-8 for the new mnemonics to match.)
MOVB = '    写字节 [r9], r0'.encode('utf-8')
INC  = '    自增 r9'.encode('utf-8')
DEC  = '    自减 r10'.encode('utf-8')

new_block = (b"""                /* zero-fill: calloc must return zeroed memory (fix 2026-08-12 host-only) */""" + NL +
b"""                mov_rr(9, 8); /* r9d = r8d = start ptr */""" + NL +
b"""                mov_r_imm(0, 0); /* eax = 0 */""" + NL +
b"""                int lcf = new_label(), lcfd = new_label();""" + NL +
b"""                set_label(lcf);""" + NL +
b"""                test_rr(10, 10); jz_rel(-1); patch_label(cp-4, lcfd, 1);""" + NL +
b"""                asm_emit("    """ + MOVB + b"""\\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x88); modrm(0,0,1); /* MOV byte [r9], al */""" + NL +
b"""                asm_emit("    """ + INC + b"""\\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3,0,1); /* inc r9 */""" + NL +
b"""                asm_emit("    """ + DEC + b"""\\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3,1,2); /* dec r10 */""" + NL +
b"""                jmp_rel(-1); patch_label(cp-4, lcf, 2);""" + NL +
b"""                set_label(lcfd);""" + NL +
b"""                /* return old counter */""" + NL +
b"""                mov_rr(0, 8);""" + NL +
b"""            } else if (!strcmp(fname, "malloc")) {""")

data = data.replace(anchor, new_block, 1)
open(p, 'wb').write(data)
print('[OK] host-only calloc zero-fill patched')
