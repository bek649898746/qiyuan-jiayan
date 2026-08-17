// @EXPECTED exit:0
// @EXPECTED out:1-1-2-1
// bug#17: 函数内 static struct 数组 brace init 误走栈帧 var_array → .data 槽 0
// (git path.c get_pathname pathname_array 模式)
#include <stdio.h>

char slop[] = "";

struct S {
    char *buf;
    int len;
};

struct S *get(void) {
    static struct S arr[2] = {
        { slop, 1 },
        { slop, 2 },
    };
    static int i = 0;
    struct S *sb = &arr[i];
    i = (i + 1) % 2;
    return sb;
}

int main(void) {
    struct S *a = get();
    struct S *b = get();
    struct S *d = get(); /* 回到 arr[0], 验证 static 持久性 */
    printf("%d-%d-%d-%d\n", a->buf != 0, a->len, b->len, d->len);
    return 0;
}
