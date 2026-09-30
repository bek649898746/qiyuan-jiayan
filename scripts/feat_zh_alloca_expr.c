// @EXPECTED out:PASS
#include <stdio.h>

/* C73 补锚 — **GCC 式 表达式形态 栈分配** `alloca(字节数)` (第 60 站, 2026-09-17)
 *
 * 背景: 甲言原生内建是**声明式** (`alloca 名, 大小;` — 声明一个栈缓冲区变量);
 *       而真实代码 (compat/nedmalloc/nedmalloc.c L940
 *       `size_t *adjustedsizes = (size_t *) alloca(elems*sizeof(size_t));`) 用 **表达式形态**
 *       ⟹ 原先报 A1001「未声明函数 'alloca'」(10 例矩阵全红)。
 * 本锚覆盖: ① 带转型 ② 不带转型 ③ 变量大小 ④ sizeof ⑤ 一个函数内多次 ⑥ 作实参 ⑦ 循环内
 *           ⑧ 对照: 甲言原生 声明式 形态仍可用
 */

static int 带转型(void) {
    int *p = (int *) alloca(4 * sizeof(int));
    p[0] = 11; p[1] = 22; p[2] = 33; p[3] = 44;
    return p[0] + p[1] + p[2] + p[3];
}

static int 不带转型(void) {
    char *p = alloca(8);
    p[0] = 5; p[1] = 7;
    return p[0] + p[1];
}

static int 变量大小(void) {
    int n = 12;
    char *p = (char *) alloca(n);
    p[0] = 1; p[11] = 2;
    return p[0] + p[11];
}

static int 多次(void) {
    char *a = (char *) alloca(8);
    char *b = (char *) alloca(8);
    a[0] = 3; b[0] = 4;
    return a[0] + b[0];
}

static int 求和数(char *p) { return p[0] + p[1]; }

static int 作实参(void) {
    char *p = (char *) alloca(8);
    p[0] = 6; p[1] = 8;
    return 求和数(p);
}

static int 循环内(void) {
    int 和 = 0;
    /* 注: 中文 `循环` 是 while 语义 (只吃一个条件) —— `循环 (i=0;i<3;i++)` 会被当
       `while (i=0)` 恒假而静默不执行; 三段式循环要用英文 `for` (见 c116 判别). */
    for (int i = 0; i < 3; i++) {
        char *p = (char *) alloca(8);
        p[0] = (char) i;
        和 += p[0];
    }
    return 和;
}

static int 声明式对照(void) {
    alloca 缓, 16;
    缓[0] = 9;
    return 缓[0];
}

int main(void) {
    if (带转型() != 110) return 1;
    if (不带转型() != 12) return 2;
    if (变量大小() != 3) return 3;
    if (多次() != 7) return 4;
    if (作实参() != 14) return 5;
    if (循环内() != 3) return 6;
    if (声明式对照() != 9) return 7;

    printf("PASS\n");
    return 0;
}
