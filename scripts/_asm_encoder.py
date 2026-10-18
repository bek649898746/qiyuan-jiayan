# -*- coding: utf-8 -*-
"""在宿主 __asm 前插入 asm 字符串编码器 (asm_enc_string)"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

p = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c'
t = open(p, 'rb').read().decode('utf-8', errors='replace')

# 在 __asm 内建前插入编码器 (6049 前的注释行)
encoder = '''
/* fix 2026-08-11 汇编本身: __asm("指令") 字符串汇编编码器 (bin_mode).
   支持内核常用指令: hlt/nop/ret/sti/cli/iretq + mov/add/sub/cmp/push/pop 寄存器/立即数.
   Intel 语法 "mov rax, rbx"; 寄存器名: rax rbx rcx rdx rsi rdi rbp rsp r8-r15 (32位: eax...). */
static int asm_reg_id(const char *s) {
    struct { const char *n; int id; } regs[] = {
        {"rax",0},{"rbx",1},{"rcx",2},{"rdx",3},{"rsi",4},{"rdi",5},{"rbp",6},{"rsp",7},
        {"eax",0},{"ebx",1},{"ecx",2},{"edx",3},{"esi",4},{"edi",5},{"ebp",6},{"esp",7},
        {"al",0},{"bl",1},{"cl",2},{"dl",3},{"sil",4},{"dil",5},{"bpl",6},{"spl",7},
        {"r8",8},{"r9",9},{"r10",10},{"r11",11},{"r12",12},{"r13",13},{"r14",14},{"r15",15},
        {"r8d",8},{"r9d",9},{"r10d",10},{"r11d",11},{"r12d",12},{"r13d",13},{"r14d",14},{"r15d",15},
    };
    for (int i = 0; i < (int)(sizeof(regs)/sizeof(regs[0])); i++) if (!strcmp(s, regs[i].n)) return regs[i].id;
    return -1;
}
static void asm_enc_string(const char *asmtext) {
    char buf[256]; strncpy(buf, asmtext, 255); buf[255] = 0;
    char *op = buf; while (*op == ' ' || *op == '\\t') op++;
    char *rest = op; while (*rest && *rest != ' ' && *rest != '\\t' && *rest != ',') rest++;
    char opc[32]; int opl = rest - op; if (opl > 31) opl = 31; memcpy(opc, op, opl); opc[opl] = 0;
    while (*rest == ' ' || *rest == '\\t' || *rest == ',') rest++;
    char *arg1 = rest; while (*arg1 == ' ' || *arg1 == '\\t') arg1++;
    /* 单操作数或无操作数指令 */
    if (!strcmp(opc, "hlt")) { b(0xF4); return; }
    if (!strcmp(opc, "nop")) { b(0x90); return; }
    if (!strcmp(opc, "ret") || !strcmp(opc, "retq")) { b(0xC3); return; }
    if (!strcmp(opc, "sti")) { b(0xFB); return; }
    if (!strcmp(opc, "cli")) { b(0xFA); return; }
    if (!strcmp(opc, "int3")) { b(0xCC); return; }
    if (!strcmp(opc, "iretq")) { b(0x48); b(0xCF); return; }
    /* push/pop reg */
    if (!strcmp(opc, "push") || !strcmp(opc, "pop")) {
        char *a1 = arg1; while (*a1 == '%') a1++;
        int r = asm_reg_id(a1);
        if (r >= 0) {
            int is64 = (a1[0] == 'r') || (a1[0] == 'e');
            if (r < 8) {
                if (!strcmp(opc, "push")) b(is64 ? (0x50 + r) : (0x50 + r)); /* push r64 = 50+rd */
                else b(is64 ? (0x58 + r) : (0x58 + r));
            } else {
                int lo = r - 8;
                if (!strcmp(opc, "push")) { b(0x41); b(0x50 + lo); }
                else { b(0x41); b(0x58 + lo); }
            }
            return;
        }
    }
    /* 双操作数: mov/add/sub/cmp/and/or/xor  dst, src */
    char *comma = arg1; while (*comma && *comma != ',') comma++;
    if (*comma == ',') {
        *comma = 0; comma++;
        while (*comma == ' ' || *comma == '\\t') comma++;
        char *dst = arg1; while (*dst == '%') dst++;
        char *src = comma; while (*src == '%') src++;
        int dr = asm_reg_id(dst), sr = asm_reg_id(src);
        /* 立即数: 0xNN 或 数字 */
        int imm = 0; int is_imm = (src[0] == '$') || (src[0] >= '0' && src[0] <= '9');
        if (is_imm) {
            if (src[0] == '$') src++;
            if (src[0] == '0' && (src[1] == 'x' || src[1] == 'X')) { imm = (int)strtol(src, 0, 16); }
            else imm = atoi(src);
            /* mov r64, imm64 / add r64, imm32 */
            if (dr >= 0) {
                int is64 = dst[0] == 'r';
                if (!strcmp(opc, "mov")) {
                    if (dr < 8) {
                        if (is64) { b(0x48); b(0xC7); b(0xC0 + dr); b4(imm); }
                        else { b(0xC7); b(0xC0 + dr); b4(imm); }
                    } else { b(0x49); b(0xC7); b(0xC0 + dr - 8); b4(imm); }
                    return;
                }
                if (!strcmp(opc, "add")) {
                    if (dr < 8) { if (is64) b(0x48); b(0x81); b(0xC0 + dr); b4(imm); }
                    else { b(0x49); b(0x81); b(0xC0 + dr - 8); b4(imm); }
                    return;
                }
                if (!strcmp(opc, "sub")) {
                    if (dr < 8) { if (is64) b(0x48); b(0x81); b(0xE8 + dr); b4(imm); }
                    else { b(0x49); b(0x81); b(0xE8 + dr - 8); b4(imm); }
                    return;
                }
                if (!strcmp(opc, "cmp")) {
                    if (dr < 8) { if (is64) b(0x48); b(0x81); b(0xF8 + dr); b4(imm); }
                    else { b(0x49); b(0x81); b(0xF8 + dr - 8); b4(imm); }
                    return;
                }
            }
        }
        /* reg, reg */
        if (dr >= 0 && sr >= 0) {
            int d64 = dst[0] == 'r', s64 = src[0] == 'r';
            if (!strcmp(opc, "mov")) {
                /* mov r64, r64 */
                if (dr < 8 && sr < 8) {
                    if (d64) b(0x48);
                    b(0x89); b(0xC0 + sr * 8 + dr);
                } else if (dr >= 8 && sr < 8) { b(0x4D); b(0x89); b(0xC0 + sr * 8 + (dr - 8)); }
                else if (dr < 8 && sr >= 8) { b(0x4C); b(0x89); b(0xC0 + (sr - 8) * 8 + dr); }
                else { b(0x4D); b(0x89); b(0xC0 + (sr - 8) * 8 + (dr - 8)); }
                return;
            }
            if (!strcmp(opc, "add") || !strcmp(opc, "sub") || !strcmp(opc, "cmp")) {
                int base = !strcmp(opc, "add") ? 0x01 : (!strcmp(opc, "sub") ? 0x29 : 0x39);
                if (dr < 8 && sr < 8) { if (d64) b(0x48); b(base); b(0xC0 + sr * 8 + dr); }
                else if (dr >= 8 && sr < 8) { b(0x4D); b(base); b(0xC0 + sr * 8 + (dr - 8)); }
                else if (dr < 8 && sr >= 8) { b(0x4C); b(base); b(0xC0 + (sr - 8) * 8 + dr); }
                else { b(0x4D); b(base); b(0xC0 + (sr - 8) * 8 + (dr - 8)); }
                return;
            }
        }
    }
    fprintf(stderr, "[ERR] __asm 未编码: %s\\n", asmtext);
}
'''
# 插入位置: __asm 内建的注释前 (6049 行)
anchor = '            /* 内建 __asm(args...): 发射任意字节到代码流'
idx = t.find(anchor)
if idx > 0:
    t = t[:idx] + encoder + '\n' + t[idx:]
    open(p, 'wb').write(t.encode('utf-8'))
    print('[OK] asm_enc_string 编码器已插入')
else:
    print('[FAIL] 插入点未找到')
