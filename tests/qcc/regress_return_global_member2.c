// @EXPECTED exit:0
// @EXPECTED out:ok
#include <stdio.h>

struct config { int a; };
struct raw_store { char x[24]; };

struct repository {
    struct config config;
    void *index;
    void *hash_algo;
    void *compat_index;
    void *compat_hash_algo;
    struct raw_store *objects;
    void *remote_state;
    char *worktree;          /* 深偏移: 7 个字段之后 */
    void *parsed_objects;
    int dummy;
};

struct repository *the_repository;

const char *get_git_work_tree(void) {
    return the_repository->worktree;
}

int main(void) {
    struct repository r;
    r.worktree = "ok";
    the_repository = &r;
    printf("%s\n", get_git_work_tree());
    return 0;
}
