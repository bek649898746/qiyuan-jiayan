// @EXPECTED exit:0
// @EXPECTED out:3
#include <stdio.h>

int get_n(int strict) {
    if (!strict) {
        int a[] = { 1, 2, 3 };
        return a[2];
    }
    return 0;
}

int main(void) {
    printf("%d\n", get_n(0));
    return 0;
}
