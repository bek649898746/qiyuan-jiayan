// @EXPECTED exit:0
// @EXPECTED out:1
// regress: 函数指针类型 cast 实参 (void*(*)(long))fn — v1 曾崩 0xC0000005 (kwset.c obstack_init)
#include <stdio.h>
#include <stdlib.h>

int take_alloc(void *(*f)(long)) {
    return f ? 1 : 0;
}

int main(void) {
    printf("%d\n", take_alloc((void *(*)(long)) malloc));
    return 0;
}
