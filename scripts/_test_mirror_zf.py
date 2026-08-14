# -*- coding: utf-8 -*-
"""Test: add zero-fill to mirror calloc branch ONLY (host stays reverted)."""
p = 'srclib_jiayan/qcc_work.jy'
data = open(p, 'rb').read()
NL = '\r\n'

anchor_old = (
    '                /* store back */' + NL +
    '                整 rel_store = data_rva_base - 0x1000 - cp - 6;' + NL +
    '                asm_emit("    存静32 [rip+%d], eax\\n", (字*)(长)(整)rel_store, 0, 0);' + NL +
    '                b(0x89); b(0x05); b4(rel_store); /* mov [rip+rel], eax */' + NL +
    '                /* return old counter */' + NL +
    '                mov_rr(0, 8);'
).encode('utf-8')
print('anchor count:', data.count(anchor_old))
if data.count(anchor_old) == 1:
    new = anchor_old + ((
        '                /* zero-fill test */' + NL +
        '                mov_rr(9, 8);' + NL +
        '                mov_r_imm(0, 0);' + NL +
        '                整 lcf2 = new_label(), lcfd2 = new_label();' + NL +
        '                set_label(lcf2);' + NL +
        '                test_rr(10, 10); jz_rel(-1); patch_label(cp-4, lcfd2, 1);' + NL +
        '                asm_emit("    写字节 [r9], r0\\n", 0, 0, 0); rex(0,0,0,1); b(0x88); modrm(0,0,1);' + NL +
        '                asm_emit("    自增 r9\\n", 0, 0, 0); rex(0,0,0,1); b(0xFF); modrm(3,0,1);' + NL +
        '                asm_emit("    自减 r10\\n", 0, 0, 0); rex(0,0,0,1); b(0xFF); modrm(3,1,2);' + NL +
        '                jmp_rel(-1); patch_label(cp-4, lcf2, 2);' + NL +
        '                set_label(lcfd2);'
    ).encode('utf-8'))
    open(p, 'wb').write(data.replace(anchor_old, new, 1))
    print('patched')
else:
    print('anchor NOT found')
