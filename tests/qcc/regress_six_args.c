// @EXPECTED exit:0
// @EXPECTED out:ok
// regress: 6 参数函数栈参传递 (fix 2026-08-19)
//   原 pdisp=56 假设 prologue 4 push (rbp=call_rsp-24), 实际 2 push (rbp=call_rsp-8)
//   → 第 5/6 参错位 16B 读垃圾 → git status path_matches_pattern_list 读垃圾 pl SEGV
#include <stdio.h>

static int sum6(int a, int b, int c, int d, int e, int f)
{
    return a + b + c + d + e + f;
}

static const char *pick6(const char *p1, const char *p2, const char *p3,
                         const char *p4, const char *p5, const char *p6)
{
    return p6; /* 第 6 参 (栈上) */
}

int main(void)
{
    if (sum6(1, 2, 3, 4, 5, 6) != 21) { printf("sum fail\n"); return 1; }
    if (sum6(10, 20, 30, 40, 50, 60) != 210) { printf("sum2 fail\n"); return 2; }
    if (sum6(100, 200, 300, 400, 500, 600) != 2100) { printf("sum3 fail\n"); return 3; }
    const char *r = pick6("a", "b", "c", "d", "e", "f");
    if (r[0] != 'f') { printf("pick fail: %s\n", r); return 4; }
    printf("ok\n");
    return 0;
}
