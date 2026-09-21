// @EXPECTED out:PASS
#include <stdio.h>

/* R231 补锚 — **文件级/全局 匿名结构体的「指针字段」读取** (第 231 站, 2026-09-21)
 *
 * 病根: 匿名结构体块的**指针字段**登记把 域型(毛利) 指向「指向的结构体」;
 *   而「静态 结构体 成员读取」的 8 字节判据用 `类型表[域型].尺寸 != 8` **间接推断**指针性
 *   ⟹ 指向的结构体恰好 8 字节时判据为假 ⟹ 读路径保留**字段地址**(&S+8) 而非字段值
 *   ⟹ 静默错值 (实测 S.p1 打出 42cfb8 = &S+8)。
 *   同族判据 4 份 (数组[i].字段 / 指针->字段 / 局部 变量.字段 / 静态 点成员),
 *   前三份或已含权威标记 `字段是否指`、或分支顺序无此歧义 —— 仅「静态 点成员」漏。
 *
 * 本锚钉四种形态一起:
 *   ① 文件级 static 匿名外层 + 内层**具名**结构体指针
 *   ② 全局(非 static) 匿名外层 + 内层**匿名**结构体指针
 *   ③ 局部 同名形态 (本来就对 —— 防「修 A 坏 B」)
 *   ④ 取址路径 &S.p1 与字段偏移 8 (与 ① 交叉验证)
 * 旧编译器 (bf30e2c / 372B257E 系) 在 ①② 上返回字段地址 ⟹ 返回 1/2。
 */

struct N { int x; char c; };                       /* 尺寸 8 —— 恰为歧义尺寸 */

static struct { int nr; struct N *p1; } S;         /* ① 外层匿名 + 内层具名 */
struct { int nr; struct { int x; char c; } *p2; } G;   /* ② 全局 + 内层匿名 */

int main(void) {
    struct { int nr; struct { int x; char c; } *p3; } L;   /* ③ 局部对照 */
    void **q;

    S.nr = 77;  S.p1 = (struct N *)0x1111;
    G.nr = 88;  G.p2 = (void *)0x2222;
    L.nr = 99;  L.p3 = (void *)0x3333;

    if ((void *)S.p1 != (void *)0x1111) return 1;   /* ① 曾是 &S+8 */
    if ((void *)G.p2 != (void *)0x2222) return 2;   /* ② 曾是 &G+8 */
    if ((void *)L.p3 != (void *)0x3333) return 3;

    q = &S.p1;                                      /* ④ 取址路径 */
    if (*q != (void *)0x1111) return 4;
    if ((long)((char *)&S.p1 - (char *)&S) != 8) return 5;

    printf("PASS\n");
    return 0;
}
