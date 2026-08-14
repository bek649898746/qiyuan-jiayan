# -*- coding: utf-8 -*-
"""Byte-safe patch: add zero-fill loop to codegen calloc branch in GBK host.
Host qcc_x86.c is GBK-encoded; all asm_emit Chinese mnemonics are GBK bytes.
Insert GBK-encoded mnemonic lines matching asm_zh vocabulary (same as memcpy
branch: 写字节/自增/自减).
"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

p = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c'
data = open(p, 'rb').read()

# Exact anchor: end of calloc branch + start of malloc branch (ASCII-only region)
NL = b'\r\n'
anchor_old = b"""                /* return old counter */""" + NL + b"""                mov_rr(0, 8);""" + NL + b"""            } else if (!strcmp(fname, "malloc")) {"""

# GBK-encoded mnemonics (same vocabulary as memcpy branch)
gbk = 'gbk'
MOVB = '    写字节 [r9], r0'.encode(gbk)
INC  = '    自增 r9'.encode(gbk)
DEC  = '    自减 r10'.encode(gbk)

new_block = (b"""                /* zero-fill: calloc must return zeroed memory (fix 2026-08-12 UB-remove) */""" + NL +
b"""                mov_rr(9, 8); /* r9d = r8d = start ptr */""" + NL +
b"""                mov_r_imm(0, 0); /* eax = 0 (fill value) */""" + NL +
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

if data.count(anchor_old) != 1:
    print(f'[FAIL] anchor count = {data.count(anchor_old)} (expected 1)')
    sys.exit(1)

data = data.replace(anchor_old, new_block, 1)
open(p, 'wb').write(data)
print('[OK] host calloc zero-fill patched (GBK-safe)')
