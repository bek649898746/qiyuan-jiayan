// @EXPECTED exit:0
// @EXPECTED out:ok
#include <stdio.h>

struct repository {
    const char *worktree;
    int dummy;
};

struct repository *the_repository;

const char *get_git_work_tree(void) {
    return the_repository->worktree;
}

int main(void) {
    struct repository r;
    r.worktree = "ok";
    r.dummy = 0;
    the_repository = &r;
    printf("%s\n", get_git_work_tree());
    return 0;
}
