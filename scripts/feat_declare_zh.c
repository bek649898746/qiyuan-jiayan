// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 declare 声明先行 接入测试 (2026-08-28): AI启动单/MoonBit#1 — 声明 名字(参数) 返回 类型;
   先声明 API 骨架, 实现后补; 未实现调用 → stub 返回 0 (缺失=警告非错误, AI 渐进补全) */

声明 未实现函数(int x) 返回 int;   /* 声明骨架, 无实现 */

int main(void) {
    /* ① 调用未实现声明 → stub 返回 0 */
    int r = 未实现函数(5);
    if (r != 0) { printf("FAIL stub: %d\n", r); return 1; }

    /* ② 声明的函数后补实现 (main 后定义), 前向调用正常 */
    声明 实现函数(int x) 返回 int;
    int r2 = 实现函数(5);
    if (r2 != 6) { printf("FAIL impl: %d\n", r2); return 1; }

    /* ③ 声明的未实现函数取地址 → 指向 stub (不报错) */
    int (*fp)(int) = 未实现函数;
    int r3 = fp(7);
    if (r3 != 0) { printf("FAIL fptr: %d\n", r3); return 1; }

    printf("PASS\n");
    return 0;
}

int 实现函数(int x) { return x + 1; }   /* 实现后补 */
