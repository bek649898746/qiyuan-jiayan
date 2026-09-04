// 结构体 >8B 数组元素赋值 修复回归 (2026-09-04 虾米, 挂账5 局部·单声明符)
// 旧: 例10 结构体数组元素整块拷贝 要求 基变量 型序标>=0 — 局部结构体数组注册 型序标 缺失
//     (typedef/声明路径) → 副癸=-1 → 漏整块拷贝 (只拷首8B/地址垃圾); 文件级全局数组有 型序标 正常
// 修复: 拷贝分支兜底 — 副癸<0 但 数组元素尺寸>8B (标量元素不可能) 即按 变量元素 整块拷贝
// 注: 逗号多声明符 (S16 a, arr[2]) / 函数内 static 数组 仍坏 → 另挂账
// @EXPECTED out:RESULT ALLOK
// @EXPECTED exit:0
#include <stdio.h>
typedef struct { long long a, b; } S16;
S16 fsgarr[2];   /* 文件级 */
int fails = 0;
#define CHK(name, got, want) do { long long gv=(long long)(got), wv=(long long)(want); if (gv != wv) { printf("FAIL %s: got %lld want %lld\n", name, gv, wv); fails++; } } while(0)
int main(void) {
    S16 src;
    S16 larr[2];     /* 单声明符 局部数组 */
    src.a = 0x1122334455667788LL; src.b = 0x8877665544332211LL;
    larr[0] = src;
    CHK("A1 局部数组元素 a", larr[0].a, 0x1122334455667788LL);
    CHK("A2 局部数组元素 b", larr[0].b, 0x8877665544332211LL);
    larr[1] = larr[0];
    CHK("A3 数组元素←数组元素 a", larr[1].a, 0x1122334455667788LL);
    CHK("A4 数组元素←数组元素 b", larr[1].b, 0x8877665544332211LL);
    fsgarr[0] = src;
    CHK("B1 文件级全局数组元素 a", fsgarr[0].a, 0x1122334455667788LL);
    CHK("B2 文件级全局数组元素 b", fsgarr[0].b, 0x8877665544332211LL);
    printf(fails ? "RESULT FAIL %d\n" : "RESULT ALLOK\n", fails);
    return fails ? 1 : 0;
}
