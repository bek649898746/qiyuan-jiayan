// @EXPECTED exit:0
// @EXPECTED out:a1-b2-c3-1
// 回归: static const struct P arr[] = { {"a",1},{"b",2},{"c",3} } 全局初始化 (fix 2026-08-17)
// git config.c fsync_component_names 同构 — 原 ginit struct 元素赋值源为立即数 → 从地址 N 拷贝 → 崩
#include <stdio.h>

struct P { const char *name; int bits; };
static const struct P arr[] = {
    { "a", 1 },
    { "b", 2 },
    { "c", 3 },
};

int main(void) {
    printf("%s%d-%s%d-%s%d-%d\n",
           arr[0].name, arr[0].bits,
           arr[1].name, arr[1].bits,
           arr[2].name, arr[2].bits,
           (int)sizeof(arr[0]));
    return 0;
}
