# -*- coding: utf-8 -*-
"""镜像同步 scanf: emit_scanf 函数 + cg 分发 + builtin 注册."""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
p = 'srclib_jiayan/qcc_work.jy'
s = open(p, encoding='utf-8').read()

emit_scanf_fn = '''
/* fix 2026-08-12 scanf: 读 stdin (GetStdHandle(-10)+ReadFile) → [rbp-4368] 缓冲,
   构造正序 args 数组, 调 _scanf_rt(fmt, args, buf, len) 解析写回. */
静 空 emit_scanf(整 nargs) {
    整 ffi = func_find("_scanf_rt");
    lea_r_mrsp(13, 8 * (nargs - 1)); /* r13 = &fmt (arg0 最先压, 最深; args 在 [r13-8..]) */
    mov_rr64(15, 4); /* r15 = pre-alignment rsp */
    asm_emit("    对齐栈\\n", 0, 0, 0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* and rsp, -16 */
    mov_r_imm(1, -10); /* ecx = -10 = STD_INPUT_HANDLE */
    sub_rsp_imm(32);
    call_iat(0); /* eax = stdin handle */
    add_rsp_imm(32);
    mov_rr64(10, 0); /* r10 = handle */
    lea_r_mbrp(12, scratch_base - cur_frame_sz - 4096); /* r12 = buf (callee-saved, ReadFile clobbers r11) */
    lea_r_mbrp(9, scratch_base + 240 - cur_frame_sz);   /* r9 = &read */
    sub_rsp_imm(48);
    mov_rr64(1, 10); mov_rr64(2, 12);
    mov_ri_ext(8, 4096);
    mov_r_imm(0, 0); mov_mrsp_reg64(32, 0); /* [rsp+32] = 0 */
    call_iat(3); /* ReadFile */
    add_rsp_imm(48);
    mov_reg_mbrp(0, scratch_base + 240 - cur_frame_sz); /* eax = bytes read */
    mov_rr64(9, 0); /* r9 = len */
    asm_emit("    取64 r1, [r13]\\n", 0, 0, 0); b(0x49); b(0x8B); b(0x4D); b(0); /* rcx = fmt */
    { 整 aoff = -(272 + 8 * nargs); /* args 数组在 printf scratch 区之下, 不碰缓冲 */
      遍 (整 k = 0; k < nargs - 1; k++) {
          asm_emit("    取64 r0, [r13%+d]\\n", (整)(-(8 + 8 * k)), 0, 0); b(0x49); b(0x8B); b(0x45); b(-(8 + 8 * k)); /* mov rax, [r13-8-8k] */
          asm_emit("    存64 [rbp%+d], r0\\n", (整)(aoff + 8 * k), 0, 0); b(0x48); b(0x89); modrm(2, 0, 5); b4(aoff + 8 * k); /* mov [rbp+aoff+8k], rax */
      }
      lea_r_mbrp(2, aoff); /* rdx = args 数组 */
    }
    mov_rr64(8, 12); /* r8 = buf */
    若 (ffi >= 0) { /* _scanf_rt(fmt, args, buf, len) */
        sub_rsp_imm(32); /* shadow */
        call_rel(0);
        patch_label(cp - 4, func_tbl[ffi].label, 0);
        add_rsp_imm(32);
    }
    mov_rr64(4, 15); /* rsp = r15 */
}
'''

# 1. 插 emit_scanf 到 emit_print 结尾 (L5021 '}' 后, 即 'mov_rr64(4, 15)' 之后的 '}')
anchor = '''    mov_rr64(4, 15); /* mov rsp, r15 �?restore original stack position */
}'''
assert anchor in s, 'anchor1 not found'
s = s.replace(anchor, anchor + emit_scanf_fn, 1)

# 2. cg 分发: printf 分支后加 scanf
old2 = '            否 若 (!strcmp(fname, "printf") || !strcmp(fname, "fprintf") || !strcmp(fname, "putstr")) { emit_print(fname, nargs); }'
if old2 not in s:
    # 镜像可能是中文/空格差异, 用宽松匹配
    import re
    m = re.search(r'否 若 \(!strcmp\(fname, "printf"\)[^}]*emit_print\(fname, nargs\); \}', s)
    if m:
        old2 = m.group(0)
    else:
        print('cg dispatch NOT FOUND')
        old2 = None
if old2:
    new2 = old2 + ' 否 若 (!strcmp(fname, "scanf")) { emit_scanf(nargs); }'
    s = s.replace(old2, new2, 1)
    print('cg dispatch patched')

# 3. builtin 注册: printf 列表加 scanf
old3 = '"printf", "fprintf", "sprintf", "snprintf", "putstr"'
if old3 in s:
    s = s.replace(old3, '"printf", "fprintf", "sprintf", "snprintf", "putstr", "scanf"', 1)
    print('builtin list patched')
else:
    print('builtin list NOT FOUND')

open(p, 'w', encoding='utf-8').write(s)
print('done')
