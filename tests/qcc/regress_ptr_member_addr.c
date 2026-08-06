// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 回归: 指针成员取址 &p->i / offsetof 惯用法 &((T*)0)->m (fix 2026-08-06)
   原 bug: ① case 11 的 &field 按 struct 槽位算地址, 指针解引用被跳过
   ② 括号表达式/类型转换无后缀链, ((T*)0)->i 的 ->i 悬空 → off=0 */
struct SA { char c; int i; };
struct SB { char a; char b; short s; int i; };

int main(void) {
    /* offsetof 惯用法: 字段偏移 */
    int off_i = (int)((char*)&((struct SA*)0)->i - (char*)0);
    if (off_i != 4) return 1;
    int off_s = (int)((char*)&((struct SB*)0)->s - (char*)0);
    if (off_s != 2) return 2;
    int off_b = (int)((char*)&((struct SB*)0)->b - (char*)0);
    if (off_b != 1) return 3;
    /* 指针变量成员取址 */
    struct SA sa; sa.c = 1; sa.i = 5;
    struct SA *p = &sa;
    int *q = &p->i;
    if (*q != 5) return 4;
    /* 值访问 */
    if (p->i != 5 || p->c != 1) return 5;
    printf("PASS\n");
    return 0;
}
