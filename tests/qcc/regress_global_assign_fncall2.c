// @EXPECTED exit:0
// @EXPECTED out:a/b
#include <stdio.h>

const char *git_work_tree_cfg = 0;

char *xgetcwd(void) {
    return "cwd";
}

int main(void) {
    const char *git_dir = "a/b/c";
    const char *git_dir_parent = 0;
    /* strrchr 模拟: 找最后一个 '/' */
    const char *p = git_dir;
    while (*p) { if (*p == '/') git_dir_parent = p; p++; }

    if (git_dir_parent) {
        /* xstrndup + real_pathdup 模拟: 造一个 "a/b" 的副本 */
        char rel[16];
        int i = 0;
        const char *q = git_dir;
        while (q < git_dir_parent) { rel[i] = *q; i++; q++; }
        rel[i] = 0;
        git_work_tree_cfg = rel;
    }
    if (!git_work_tree_cfg)
        git_work_tree_cfg = xgetcwd();
    printf("%s\n", git_work_tree_cfg);
    return 0;
}
