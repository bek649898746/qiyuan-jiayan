// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 回归: 深层嵌套循环 + goto 跳转 */
int main(void) {
    int sum = 0;
    int i, j, k;
    /* 三层嵌套循环计数 */
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            for (k = 0; k < 3; k++) {
                sum++;
            }
        }
    }
    if (sum != 27) return 1;   /* 3*3*3 */

    /* 嵌套循环 + continue/break 混合
       continue 只跳 j==2; j==3 仍执行 (标准C). break 在 i==3 时跳出内层
       i=0: 0+1+3=4  i=1: 10+11+13=34  i=2: 20+21+23=64  i=3: break => 102 */
    sum = 0;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            if (j == 2) continue;     /* 只跳过 j==2 */
            if (i == 3) break;        /* i==3 时跳出内层 */
            sum = sum + (i * 10 + j);
        }
    }
    if (sum != 102) return 2;

    /* goto 跳出双层循环 (带标签) */
    sum = 0;
    for (i = 0; i < 10; i++) {
        for (j = 0; j < 10; j++) {
            if (i == 2 && j == 3) goto done;
            sum++;
        }
    }
done:
    /* 跳到 (2,3): 前 2 整行 2*10=20 + 本行 3 个 = 23 */
    if (sum != 23) return 3;

    /* goto 前向/后向跳转 */
    i = 0;
    goto check;
loop:
    i++;
check:
    if (i < 5) goto loop;
    if (i != 5) return 4;

    printf("PASS\n");
    return 0;
}
