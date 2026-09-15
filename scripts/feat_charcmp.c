// @EXPECTED out:digit
#include <stdio.h>
int main(void) {
    const char *s = "7";
    char c = s[0];
    printf("c=%d (0x%x)\n", c, c);
    if (c >= '1' && c <= '9') { printf("digit\n"); return 0; }
    printf("not digit\n");
    return 1;
}
