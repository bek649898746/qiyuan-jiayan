/* 启元 · 甲言 · 龙芯 828 — LoongArch 交叉编译回归测试 1
 * 验证层级：源码级交叉编译（指令生成/ABI/ELF 由龙芯 GCC 完成，
 *            甲言仅提供符合自身语法规范的源码）
 * 大哥：郑宇和  |  虾米：郑启元  |  种子：828
 */
#include <stdio.h>

int seed = 828;                     /* 种子 */
int g_counter = 0;

/* 甲言风格函数 */
int square(int x) { return x * x; }

int add3(int a, int b, int c) { return a + b + c; }

int bump(int n) { g_counter += n; return g_counter; }

/* 自举不动点哈希前缀（甲言自举里程碑 dc34276fa5e20fc4） */
int checksum(void) {
    unsigned long h = 0;
    const char *s = "dc34276fa5e20fc4";
    for (int i = 0; s[i]; i++) h = h * 131 + s[i];
    return (int)(h % 1000);
}

int main(void) {
    printf("seed=%d, square(seed)=%d\n", seed, square(seed));
    printf("add3(1,2,3)=%d\n", add3(1, 2, 3));
    bump(10);
    printf("bump(10) -> g_counter=%d\n", bump(5));
    printf("fixpoint checksum=%d\n", checksum());
    return 0;
}
