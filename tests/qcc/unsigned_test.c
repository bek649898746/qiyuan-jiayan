// @EXPECTED exit:0
/* M1 regression: unsigned comparison/division/modulo semantics (fix 2026-08-06) */
#include <stdio.h>

int main(void) {
    unsigned a = 0xFFFFFFFFu;
    unsigned b = 5u;
    int ok = 1;

    /* comparison (was signed → wrong for high-bit operands) */
    ok = ok && (0xFFFFFFFFu > 0);
    ok = ok && !(0xFFFFFFFFu < 0);
    ok = ok && (a >= a);
    ok = ok && (b <= 5u);
    ok = ok && (a > b);       /* 0xFFFFFFFF > 5 unsigned */
    ok = ok && (b < a);
    ok = ok && (a != b);
    ok = ok && (b == 5u);

    /* division / modulo (was cqo→unsigned-ized; cdq fix) */
    ok = ok && (0xFFFFFFFFu / 2 == 0x7FFFFFFFu);
    ok = ok && (0xFFFFFFFFu % 2 == 1u);
    ok = ok && (a / 2 == 2147483647u);
    ok = ok && (a % 2 == 1u);

    /* signed must stay signed */
    ok = ok && (-7 / 2 == -3);
    ok = ok && (-7 % 2 == -1);

    /* if() with unsigned condition */
    if (0xFFFFFFFFu > 0) { ok = ok && 1; } else { ok = 0; }

    if (ok) printf("unsigned PASS\n"); else printf("unsigned FAIL\n");
    return ok ? 0 : 1;
}
