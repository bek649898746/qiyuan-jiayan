// @EXPECTED out:OK
// 雷2 回归: 合法类型转换必须通过 (0/NULL 赋指针, char[]/char* 收字符串, 显式 cast 豁免)
#include <stdio.h>
int main(void) {
    int *p = 0;
    char buf[16] = "abc";
    char *s = "def";
    int i = (int)(long long)p;
    if (p != 0 || i != 0) { printf("FAIL\n"); return 1; }
    printf("OK\n");
    return 0;
}
