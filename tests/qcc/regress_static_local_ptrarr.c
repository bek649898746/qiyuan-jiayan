// @EXPECTED exit:0
// @EXPECTED out:/.git-.git
#include <stdio.h>

static const char *get_suffix(int i) {
    static const char *suffix[] = { "/.git", "", ".git/.git", ".git", NULL };
    return suffix[i];
}

int main(void) {
    printf("%s-%s\n", get_suffix(0), get_suffix(3));
    return 0;
}
