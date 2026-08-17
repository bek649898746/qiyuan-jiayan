// @EXPECTED exit:0
// @EXPECTED out:cwd
#include <stdio.h>

const char *git_work_tree_cfg = 0;

char *xgetcwd(void) {
    return "cwd";
}

int main(void) {
    if (!git_work_tree_cfg)
        git_work_tree_cfg = xgetcwd();
    printf("%s\n", git_work_tree_cfg);
    return 0;
}
