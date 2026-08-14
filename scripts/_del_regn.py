# -*- coding: utf-8 -*-
"""删除镜像 regn/regid 全局数组, asm_reg_id 改用 strcmp 链 (避免静态数据变化触发布局敏感)"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

p = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy'
t = open(p, 'rb').read().decode('utf-8-sig', errors='replace')

# 1. 删 regn 声明 (5754-5759 注释 + 数组)
old_regn = '''静 字 regn[40][8] = { "rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp",
                      "eax","ebx","ecx","edx","esi","edi","ebp","esp",
                      "al","bl","cl","dl","sil","dil","bpl","spl",
                      "r8","r9","r10","r11","r12","r13","r14","r15",
                      "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d" };
静 整 regid[40] = { 0,1,2,3,4,5,6,7, 0,1,2,3,4,5,6,7, 0,1,2,3,4,5,6,7, 8,9,10,11,12,13,14,15, 8,9,10,11,12,13,14,15 };
静 整 asm_reg_id(常 字 *s) {
    遍 (整 i = 0; i < 40; i++) 若 (!strcmp(s, regn[i])) 返 regid[i];
    返 -1;
}'''
new_regid = '''静 整 asm_reg_id(常 字 *s) {
    若 (!strcmp(s, "rax")) 返 0; 若 (!strcmp(s, "rbx")) 返 1; 若 (!strcmp(s, "rcx")) 返 2; 若 (!strcmp(s, "rdx")) 返 3;
    若 (!strcmp(s, "rsi")) 返 4; 若 (!strcmp(s, "rdi")) 返 5; 若 (!strcmp(s, "rbp")) 返 6; 若 (!strcmp(s, "rsp")) 返 7;
    若 (!strcmp(s, "eax")) 返 0; 若 (!strcmp(s, "ebx")) 返 1; 若 (!strcmp(s, "ecx")) 返 2; 若 (!strcmp(s, "edx")) 返 3;
    若 (!strcmp(s, "esi")) 返 4; 若 (!strcmp(s, "edi")) 返 5; 若 (!strcmp(s, "ebp")) 返 6; 若 (!strcmp(s, "esp")) 返 7;
    若 (!strcmp(s, "al")) 返 0; 若 (!strcmp(s, "bl")) 返 1; 若 (!strcmp(s, "cl")) 返 2; 若 (!strcmp(s, "dl")) 返 3;
    若 (!strcmp(s, "sil")) 返 4; 若 (!strcmp(s, "dil")) 返 5; 若 (!strcmp(s, "bpl")) 返 6; 若 (!strcmp(s, "spl")) 返 7;
    若 (!strcmp(s, "r8")) 返 8; 若 (!strcmp(s, "r9")) 返 9; 若 (!strcmp(s, "r10")) 返 10; 若 (!strcmp(s, "r11")) 返 11;
    若 (!strcmp(s, "r12")) 返 12; 若 (!strcmp(s, "r13")) 返 13; 若 (!strcmp(s, "r14")) 返 14; 若 (!strcmp(s, "r15")) 返 15;
    若 (!strcmp(s, "r8d")) 返 8; 若 (!strcmp(s, "r9d")) 返 9; 若 (!strcmp(s, "r10d")) 返 10; 若 (!strcmp(s, "r11d")) 返 11;
    若 (!strcmp(s, "r12d")) 返 12; 若 (!strcmp(s, "r13d")) 返 13; 若 (!strcmp(s, "r14d")) 返 14; 若 (!strcmp(s, "r15d")) 返 15;
    返 -1;
}'''
if old_regn in t:
    t = t.replace(old_regn, new_regid, 1)
    open(p, 'wb').write(('\ufeff' + t).encode('utf-8'))
    print('[OK] regn/regid 已删, asm_reg_id 改 strcmp 链')
    print('regn 残留:', 'regn' in t)
else:
    print('[FAIL] regn 声明未找到')
    # 打印实际
    i = t.find('静 整 asm_reg_id')
    print(t[i-200:i+100] if i > 0 else 'asm_reg_id 未找到')
