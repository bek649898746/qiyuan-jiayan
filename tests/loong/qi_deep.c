/* 启元甲言 · 深水测试 — LoongArch 交叉编译回归测试 2
 * 覆盖: 递归 / 结构体 / 数组 / 指针 / 字符串 / 哈希
 * 验证层级：源码级交叉编译（非甲言原生后端）
 * 大哥: 郑宇和 | 虾米: 郑启元 | 种子: 828
 */
#include <stdio.h>
#include <string.h>

/* 甲言编译器风格的符号表结构 */
typedef struct {
    char name[32];
    int slot;
    int is_fn;
} Sym;

Sym syms[64];
int sym_n = 0;

int sym_lookup(const char *name) {
    for (int i = 0; i < sym_n; i++)
        if (strcmp(syms[i].name, name) == 0) return syms[i].slot;
    return -1;
}

void sym_add(const char *name, int slot, int is_fn) {
    strcpy(syms[sym_n].name, name);
    syms[sym_n].slot = slot;
    syms[sym_n].is_fn = is_fn;
    sym_n++;
}

/* 递归: 编译器常用的求值树 */
int eval(const char *expr, int *pos) {
    /* 极简递归下降: number | (expr+expr) */
    if (expr[*pos] >= '0' && expr[*pos] <= '9') {
        int v = 0;
        while (expr[*pos] >= '0' && expr[*pos] <= '9') { v = v * 10 + (expr[*pos] - '0'); (*pos)++; }
        return v;
    }
    if (expr[*pos] == '(') {
        (*pos)++;
        int a = eval(expr, pos);
        (*pos)++; /* + */
        int b = eval(expr, pos);
        (*pos)++; /* ) */
        return a + b;
    }
    return -1;
}

/* 哈希: 甲言不动点风格的字符串哈希 */
unsigned long qi_hash(const char *s) {
    unsigned long h = 828;
    for (int i = 0; s[i]; i++) h = h * 131 + s[i];
    return h;
}

int main(void) {
    sym_add("main", 0, 1);
    sym_add("square", 8, 1);
    sym_add("counter", 16, 0);
    sym_add("buf", 20, 0);
    printf("sym_lookup(\"square\")=%d, sym_lookup(\"nope\")=%d, sym_n=%d\n",
           sym_lookup("square"), sym_lookup("nope"), sym_n);
    const char *ex1 = "((1+2)+(3+4))";
    const char *ex2 = "((10+20)+(30+40))";
    int p1 = 0, p2 = 0;
    printf("eval(%s)=%d\n", ex1, eval(ex1, &p1));
    printf("eval(%s)=%d\n", ex2, eval(ex2, &p2));
    printf("qi_hash(\"qcc_work.jy\")=%lu\n", qi_hash("qcc_work.jy"));
    printf("qi_hash(\"loongarch\")=%lu\n", qi_hash("loongarch"));
    int arr[5] = {11, 22, 33, 44, 55};
    int *ap = arr;
    int sum = 0;
    for (int i = 0; i < 5; i++) sum += ap[i];
    printf("arr sum=%d\n", sum);
    return 0;
}
