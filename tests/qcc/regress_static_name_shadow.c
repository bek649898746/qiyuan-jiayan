// @EXPECTED exit:0
// @EXPECTED out:/.git-.git
#include <stdio.h>

static const char *get_suffix(int i) {
    static const char *suffix[] = { "/.git", "", ".git/.git", ".git", NULL };
    return suffix[i];
}

static void other_fn(void) {
    char *suffix = 0; /* 同名本地, 遮蔽静态数组 */
    (void)suffix;
}

int main(void) {
    other_fn();
    printf("%s-%s\n", get_suffix(0), get_suffix(3));
    return 0;
}
