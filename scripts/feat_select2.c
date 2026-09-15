// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 select (择收) 接入测试: 多路条件分派 — 第一个可用条件执行对应分支
   语法: 择收 (e1, e2, ...) { 例 0: b0; 例 1: b1; 缺: bd; } → if(e1)b0 else if(e2)b1 else bd
   gcc 不识别 择收/例/缺 — 只用 qcc */

int main(void) {
    /* 1. 第一路可用 */
    {
        int a = 1, b = 0;
        int r = -2;
        择收 (a > 0, b > 0) {
        例 0: r = 100;
        例 1: r = 200;
        缺: r = -1;
        }
        if (r != 100) return 1;
    }

    /* 2. 第二路可用 (第一路假) */
    {
        int a = 0, b = 1;
        int r = -2;
        择收 (a > 0, b > 0) {
        例 0: r = 100;
        例 1: r = 200;
        缺: r = -1;
        }
        if (r != 200) return 2;
    }

    /* 3. 全部不可用 → default */
    {
        int a = 0, b = 0;
        int r = -2;
        择收 (a > 0, b > 0) {
        例 0: r = 100;
        例 1: r = 200;
        缺: r = -1;
        }
        if (r != -1) return 3;
    }

    /* 4. 三路 */
    {
        int a = 0, b = 0, c = 1;
        int r = -2;
        择收 (a > 0, b > 0, c > 0) {
        例 0: r = 100;
        例 1: r = 200;
        例 2: r = 300;
        缺: r = -1;
        }
        if (r != 300) return 4;
    }

    /* 5. 通道接收语义模拟: 接收条件 + 值 */
    {
        int data[2] = {7, 0};
        int avail[2] = {1, 0};
        int x = 0;
        int r = -2;
        择收 (avail[0], avail[1]) {
        例 0: x = data[0]; r = 100;
        例 1: x = data[1]; r = 200;
        缺: r = -1;
        }
        if (r != 100 || x != 7) return 5;
    }

    /* 6. 优先级: 多路可用选第一 */
    {
        int r = -2;
        择收 (1, 1) {
        例 0: r = 100;
        例 1: r = 200;
        缺: r = -1;
        }
        if (r != 100) return 6;
    }

    printf("s1=%d s2=%d s3=%d s4=%d\n", 100, 200, -1, 300);
    printf("PASS\n");
    return 0;
}
