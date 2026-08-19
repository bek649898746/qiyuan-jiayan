// @EXPECTED exit:0
// @EXPECTED out:1
// regress: 函数指针类型 cast 实参 (void*(*)(long))fn — v1 曾崩 0xC0000005 (kwset.c obstack_init)
// fix 2026-08-19: 原用 builtin malloc 取地址 → 单文件模式无真实函数体, 靠 rax 垃圾碰运气; 改用用户函数 (健壮, 意图不变)
#include <stdio.h>

static void *my_alloc(long n) { (void)n; return (void *)0x1; }

int take_alloc(void *(*f)(long)) {
    return f ? 1 : 0;
}

int main(void) {
    printf("%d\n", take_alloc((void *(*)(long)) my_alloc));
    return 0;
}
