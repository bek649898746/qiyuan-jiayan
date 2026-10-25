// @EXPECTED exit:0
// @EXPECTED silent:0
/* ⚠️ 本锚兼作「裸 char 无符号语义守卫」（2026-09-25 标注 · 审计岗建议）
 *   `if (gu.c[0] != 0xDD) return 4;` 钉死：甲言的裸 `char` 必须按**无符号**读。
 *   定案见 `memory/atlas/_SETTLED.md` S-1（C 标准里 char 符号性实现定义；甲言选无符号 ≡ gcc -funsigned-char）。
 *   历史：本锚已两次拦住「把裸 char 改成 signed」的改动（C5 刀2 · 第 357 站 t2）——
 *   **它报 rc=4 是守护生效，不是普通回归**。
 *   ⚠️ 约束：闸 `run_tests_zh.py` L1699 只读文件**前 600 字节**解析 @EXPECTED ⟹
 *   `// @EXPECTED` 必须留在**第一行**，本注释块**不得再往上插行**。
 */
// 回归: union 成员重叠 (2026-08-09 语言特性覆盖)
union U { int i; char c[4]; };
union U gu;
int main(void) {
    union U u;
    u.i = 0x12345678;
    if (u.c[0] != 0x78) return 1;
    if (u.c[1] != 0x56) return 2;
    if (u.c[3] != 0x12) return 3;
    gu.i = 0xAABBCCDD;
    if (gu.c[0] != 0xDD) return 4;
    if (gu.c[2] != 0xBB) return 5;
    return 0;
}
