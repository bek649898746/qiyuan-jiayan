#include <stdio.h>
/* 2026-09-08 块级 packed + 块级局部 struct 字段类型 (防回归):
   ① 块级 __attribute__((packed)) struct 定义+字段访问 (原编译崩 "未定义函数 打包")
   ② 块级局部 struct 字段类型判定 (short/int/long 原落默认 4B, packed 无填充暴露)
   ③ 嵌套块 (if/for) + 显式中文 打包 构
   @EXPECT out:BLKPASS */
int main(void) {
    /* ① 函数体块级 packed */
    __attribute__((packed)) struct F3 { char a; int b; short c; };
    if (sizeof(struct F3) != 7) { printf("F3=%d\n", (int)sizeof(struct F3)); return 1; }
    struct F3 x; x.a = 1; x.b = 2; x.c = 3;
    if (x.a != 1 || x.b != 2 || x.c != 3) return 2;
    /* ② 块级普通 struct 字段类型: 短 2B (原落默认4B, packed 无填充暴露) */
    struct W { char a; short s; int i; };
    if (sizeof(struct W) != 8) { printf("W=%d\n", (int)sizeof(struct W)); return 3; }
    __attribute__((packed)) struct P { short a; int b; char c; };
    if (sizeof(struct P) != 7) { printf("P=%d\n", (int)sizeof(struct P)); return 4; }
    /* 注: 块级 struct 的 long/long long 字段另有词法分类缺陷 (long 词型=词变量非型词), 挂账中 */
    /* ③ 嵌套块 */
    int n = 1;
    if (n) {
        __attribute__((packed)) struct N3 { char a; int b; short c; };
        if (sizeof(struct N3) != 7) { printf("N3=%d\n", (int)sizeof(struct N3)); return 5; }
        struct N3 y; y.a = 1; y.b = 2; y.c = 3;
        if (y.a != 1 || y.b != 2 || y.c != 3) return 6;
    }
    for (int i = 0; i < 1; i++) {
        打包 构 LZ { 字 a; 整 b; };
        if (sizeof(构 LZ) != 5) { printf("LZ=%d\n", (int)sizeof(构 LZ)); return 7; }
    }
    printf("BLKPASS\n");
    return 0;
}
