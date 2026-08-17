// @EXPECTED exit:0
// @EXPECTED out:/.git-/.git
#include <stdio.h>

static const char *get_suffix(int strict) {
    if (!strict) {
        static const char *suffix[] = { "/.git", "", ".git/.git", ".git", NULL };
        return suffix[0];
    }
    return "/.git";
}

int main(void) {
    printf("%s-%s\n", get_suffix(0), get_suffix(1));
    return 0;
}
