#include <stdio.h>

/* 甲言 可变借用 &mut 负例验证 (2026-08-28): 机制层借用族 P0 #3 — 每段都应被编译期 [ERR] 拦截
   (单独编译, 期望 exit!=0; 用于验证独占语义) */

int main(void) {
    int x = 10;
    /* NEG-1: 可变借用后读取 x → [ERR] */
    可变借用 x;
    int r = x + 1;
    (void)r;
    放还 x;
    printf("PASS\n");
    return 0;
}
