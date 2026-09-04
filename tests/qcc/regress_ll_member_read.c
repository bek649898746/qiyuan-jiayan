// @EXPECTED exit:0
// @EXPECTED out:OK
/* 修复 2026-09-04: 局部帧 8B 整成员读 截断缺陷 (例15 局部路径 64 装载漏 节长标, 值被当 32 位截高半;
   表达式是否长 漏 type15 → 算术按 32)。覆盖: 局部/全局/数组 成员比较 + 算术(加长整变量/乘/加长整常量);
   8B 结构体传值/赋值; 8B+int 邻接字段完好 */
#include <stdio.h>

struct S { long long x; };   /* 8B 结构体 (byval/赋值路径) */
struct T { long long x; int a; }; /* 邻接字段 */

static int chk(struct S byv) {
    if (byv.x != 0x1122334400000000LL) { printf("FAIL byval %llx\n", (unsigned long long)byv.x); return 1; }
    struct S lv = byv;
    if (lv.x != 0x1122334400000000LL) { printf("FAIL sassign %llx\n", (unsigned long long)lv.x); return 1; }
    return 0;
}

int main(void) {
    struct S gs2;
    struct S ls;
    struct T t;
    gs2.x = 0x1122334400000000LL;
    ls.x = 0x1122334400000000LL;
    t.x = 0x1122334400000000LL;
    t.a = 7;

    /* 比较 (全局/局部) */
    if (gs2.x != 0x1122334400000000LL) { printf("FAIL g cmp\n"); return 1; }
    if (ls.x != 0x1122334400000000LL) { printf("FAIL l cmp %llx\n", (unsigned long long)ls.x); return 1; }

    /* 算术: + 长整 变量 / * / + 长整 常量 */
    long long v = 1;
    if ((ls.x + v) != 0x1122334400000001LL) { printf("FAIL add-var\n"); return 1; }
    if ((ls.x * 2) != 0x2244668800000000LL) { printf("FAIL mul\n"); return 1; }
    if ((ls.x + 0x1122334400000001LL) != 0x2244668800000001LL) { printf("FAIL add-lc\n"); return 1; }

    /* 传值参数 / 结构体赋值局部 (8B) */
    if (chk(ls)) return 1;

    /* 局部数组元素 */
    struct S arr[2];
    arr[1].x = 0x1122334400000000LL;
    if (arr[1].x != 0x1122334400000000LL) { printf("FAIL arr\n"); return 1; }

    /* 邻接 int 字段完好 + T 成员算术 */
    if (t.a != 7) { printf("FAIL int field\n"); return 1; }
    if ((t.x + v) != 0x1122334400000001LL) { printf("FAIL T add-var\n"); return 1; }

    printf("OK\n");
    return 0;
}
