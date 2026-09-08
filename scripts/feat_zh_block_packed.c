#include <stdio.h>
/* 2026-09-08 块级 packed + 块级局部 struct 字段类型 (防回归):
   ① 块级 __attribute__((packed)) struct 定义+字段访问 (原编译崩 "未定义函数 打包")
   ② 块级局部 struct 字段类型判定 (short/int/long 原落默认 4B, packed 无填充暴露)
   ③ 嵌套块 (if/for) + 显式中文 打包 构
   @EXPECT out:BLKPASS */
struct TPoint { int x; };
typedef struct TPoint TP;
typedef unsigned short MyUS;
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
    /* ③b long long 块级 (double-long 词位消费 2026-09-08; 注: unsigned 前缀组合另有缺陷挂账) */
    __attribute__((packed)) struct PL { char a; long long ll; };
    if (sizeof(struct PL) != 9) { printf("PL=%d\n", (int)sizeof(struct PL)); return 8; }
    struct PL pl; pl.a = 1; pl.ll = 1234567890123LL;
    if (pl.ll != 1234567890123LL) { printf("PL ll=%lld\n", pl.ll); return 9; }
    struct NW { char a; long long ll; };
    if (sizeof(struct NW) != 16) { printf("NW=%d\n", (int)sizeof(struct NW)); return 10; }
    /* ③c unsigned short 块级无符号标 (2026-09-08: L8018 FSR 后值劣=1, 原局部字段 2B 读符号扩展错) */
    struct LU { char a; unsigned short us; };
    struct LU lu; lu.a = 1; lu.us = 65535;
    if (lu.us != 65535) { printf("LU us=%u\n", lu.us); return 11; }
    /* ③d typedef 字段 (2026-09-09: 块级具名 struct 原无 typedef 判定 → 幻影字段 + 类型丢 A9001) */
    struct TB { TP pt; MyUS tus; };
    struct TB tb; tb.pt.x = 6; tb.tus = 65535;
    if (tb.pt.x != 6) { printf("TB pt\n"); return 12; }
    if (tb.tus != 65535) { printf("TB tus=%u\n", tb.tus); return 13; }
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
