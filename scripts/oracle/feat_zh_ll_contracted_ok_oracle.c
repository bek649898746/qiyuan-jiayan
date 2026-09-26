/* oracle for scripts/feat_zh_ll_contracted_ok.c — C 语法逐值对照 (供 gcc 跑)
   R256 (2026-09-26 启元 · 第 364 站 round 1): 连写 `长长` 词法层展开为 `长 长`.
   甲言侧的 `长长` 等价于 C 的 `long long`; 输出的字符串与甲言锚逐字相同. */
#include <stdio.h>

struct LLBox { long long a, b; };
struct LLOne { long long only; };

int main(void) {
    long long x;
    long long arr[3];
    if (sizeof(struct LLBox) != 16) { printf("FAIL box %d\n", (int)sizeof(struct LLBox)); return 1; }
    if (sizeof(struct LLOne) != 8)  { printf("FAIL one %d\n", (int)sizeof(struct LLOne)); return 2; }
    if (sizeof(arr) != 24)          { printf("FAIL arr %d\n", (int)sizeof(arr)); return 3; }
    if (sizeof(x) != 8)             { printf("FAIL sc %d\n", (int)sizeof(x)); return 4; }
    x = 1;
    x = x << 32;
    if (x == 0) { printf("FAIL hi_zero\n"); return 5; }
    printf("LLOK sz=%d arr=%d sc=%d hi=1\n", (int)sizeof(struct LLBox), (int)sizeof(arr), (int)sizeof(x));
    return 0;
}
