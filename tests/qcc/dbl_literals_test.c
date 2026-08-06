// @EXPECTED exit:0
/* M3 regression: 大量 double 字面量（>512 个）不再静默变 0.0 (fix 2026-08-06) */
#include <stdio.h>

int main(void) {
    double d0 = 1.25;
    double d1 = 2.25;
    double d2 = 3.25;
    double d3 = 4.25;
    double d4 = 5.25;
    double d5 = 6.25;
    double d6 = 7.25;
    double d7 = 8.25;
    double d8 = 9.25;
    double d9 = 10.25;
    double d508 = 509.25;
    double d509 = 510.25;
    double d510 = 511.25;
    double d511 = 512.25;
    double d512 = 513.25;
    double d513 = 514.25;
    double d514 = 515.25;
    double d515 = 516.25;
    double d516 = 517.25;
    double d517 = 518.25;
    double d518 = 519.25;
    double d519 = 520.25;
    double sentinel = 12345.6789;

    if (d0 != 1.25) return 1;
    if (d9 != 10.25) return 1;
    if (d511 != 512.25) return 1;
    if (d512 != 513.25) return 1;   /* 原 512 守卫下静默 0.0 */
    if (d519 != 520.25) return 1;
    if (sentinel != 12345.6789) return 1;
    printf("dbl_literals PASS\n");
    return 0;
}
