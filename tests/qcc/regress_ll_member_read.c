// @EXPECTED exit:0
// @EXPECTED out:OK
/* 修复 2026-09-04: 局部帧 8B 整成员读 截断缺陷 (例15 局部路径 64 装载漏 节长标, 值被当 32 位截高半;
   表达式是否长 漏 type15 → 算术按 32)。覆盖: 局部/全局/数组 成员比较 + 算术(加长整变量/乘/加长整常量);
   8B 结构体传值/赋值; 8B+int 邻接字段完好 */
#include <stdio.h>

struct S { long long x; };   /* 8B 结构体 (byval/赋值路径) */
struct T { long long x; int a; }; /* 邻接字段 */
/* B4 (2026-09-09): >8B byval 形参 返回 场景 */
struct S16 { long long a; long long b; };
struct S32 { long long a; long long b; long long c; long long d; };
static struct S16 id16(struct S16 v) { return v; }   /* return 形参 (原 sret 源错取槽内存) */
static struct S32 bump32(struct S32 v) { v.a += 10; v.b += 20; v.c += 30; v.d += 40; return v; }

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

    /* B3 防回归 (2026-09-09): 32 位 int 字面量 覆写 8B 成员 → store 必须 64 位符号扩展
       清高位 (原 写路径分叉: 局部 member + 32 位字面量 store 曾走 32 位 mov → 高位残留) */
    ls.x = 0x1122334400000000LL;
    ls.x = 3;
    if (ls.x != 3) { printf("FAIL b3a %llx\n", (unsigned long long)ls.x); return 1; }
    ls.x = 0x1122334400000000LL;
    ls.x = -1;                                   /* 负 32 位字面量 → 符号扩展 */
    if (ls.x != -1) { printf("FAIL b3b %llx\n", (unsigned long long)ls.x); return 1; }
    ls.x = 0x1122334400000000LL;
    ls.x += 0;                                   /* 复合 += int 常量不丢高位 */
    if (ls.x != 0x1122334400000000LL) { printf("FAIL b3c %llx\n", (unsigned long long)ls.x); return 1; }
    t.x = 0x1122334400000000LL;                  /* 非零偏移字段 (x 前有 int a? 不, x 在 a 前) */
    t.x = 42;
    if (t.x != 42) { printf("FAIL b3d %llx\n", (unsigned long long)t.x); return 1; }

    /* B4 防回归 (2026-09-09): >8B 结构体 byval 形参 直接返回 (return 形参) —
       sret 拷贝源须是 槽内指针指向的调用方副本, 非槽内存 (槽=8B 指针+邻居垃圾) */
    struct S16 x16; x16.a = 0x1122334400000000LL; x16.b = 0x5566778800000000LL;
    struct S16 r16 = id16(x16);                  /* return 形参 */
    if (r16.a != x16.a || r16.b != x16.b) { printf("FAIL b4a16 %llx %llx\n", (unsigned long long)r16.a, (unsigned long long)r16.b); return 1; }
    struct S32 x32; x32.a = 1; x32.b = 2; x32.c = 3; x32.d = 4;
    struct S32 r32 = bump32(x32);                /* 形参 += 修改 + return */
    if (r32.a != 11 || r32.b != 22 || r32.c != 33 || r32.d != 44) { printf("FAIL b4b32 %lld %lld %lld %lld\n", (long long)r32.a, (long long)r32.b, (long long)r32.c, (long long)r32.d); return 1; }
    if (x32.a != 1) { printf("FAIL b4c caller-modified\n"); return 1; }

    printf("OK\n");
    return 0;
}
