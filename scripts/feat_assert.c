#include <stdio.h>

/* 甲言断言 (断言) 接入测试: 断言(expr); → if (expr==0) { exit(1); }
   本测试: 断言通过场景 (程序继续); 失败场景单独验证 exit 1 */

static int add(int a, int b) { return a + b; }

int main(void) {
    /* 1. 简单断言通过 */
    int x = 5;
    断言(x == 5);
    断言(x > 0);
    断言(x != 0);

    /* 2. 表达式断言 */
    断言(2 + 3 == 5);
    断言(add(1, 2) == 3);

    /* 3. 循环内断言 */
    {
        int sum = 0;
        for (int i = 0; i < 5; i++) {
            sum += i;
            断言(sum >= 0);
        }
        if (sum != 10) return 1;
    }

    /* 4. 边界断言 (等值) */
    断言(x >= 5);
    断言(x <= 5);

    /* 5. 函数内断言 */
    {
        int r = add(7, 8);
        断言(r == 15);
        if (r != 15) return 2;
    }

    printf("a1=%d a2=%d sum=%d\n", 1, 1, 10);
    printf("PASS\n");
    return 0;
}
