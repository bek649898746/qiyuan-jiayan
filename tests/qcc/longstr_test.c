// @EXPECTED exit:0
/* str_tbl 2048 回归: >510 字符单个字面量不再报错 (fix 2026-08-06) */
#include <stdio.h>

int main(void) {
    const char *s = "ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJabcdefghijabcdefghijabcdefghijabcdefghijabcdefghij01234567890123456789012345678901234567890123456789ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJabcdefghijabcdefghijabcdefghijabcdefghijabcdefghij01234567890123456789012345678901234567890123456789ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJabcdefghijabcdefghijabcdefghijabcdefghijabcdefghij01234567890123456789012345678901234567890123456789ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJabcdefghijabcdefghijabcdefghijabcdefghijabcdefghij01234567890123456789012345678901234567890123456789";

    int len = 0;
    while (s[len]) len = len + 1;
    if (len != 600) return 1;
    if (s[0] != 'A') return 1;
    if (s[599] != '9') return 1;
    if (s[250] != '0') return 1;
    printf("longstr PASS (%d chars)\n", len);
    return 0;
}
