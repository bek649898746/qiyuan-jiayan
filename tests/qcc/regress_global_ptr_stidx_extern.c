/* 复现2: extern struct X *p 的 st_idx 注册 (object-file.c the_repository 场景) */
#include <stdio.h>

struct object_id { char hash[20]; int algo; };

struct git_hash_algo {
    const char *name;
    int format_id;
    int rawsz;
    int hexsz;
    struct object_id *null_oid;
};

struct repository {
    int dummy[10];
    struct git_hash_algo *hash_algo;
};

extern struct repository *the_repository;

struct object_id null_oid_obj = { {0}, 0 };

const struct object_id *null_oid(void);

struct git_hash_algo hash_algos[2] = {
    { "sha1", 0x73686131, 20, 40, &null_oid_obj },
    { "sha256", 0x73323536, 32, 64, 0 }
};

struct repository the_repo = { {0}, &hash_algos[0] };

struct repository *the_repository = &the_repo;

const struct object_id *null_oid(void)
{
    return the_repository->hash_algo->null_oid;
}

int main(void)
{
    printf("null_oid() = %p (expect %p)\n", (void*)null_oid(), (void*)&null_oid_obj);
    if (null_oid() != &null_oid_obj) {
        printf("FAIL: return value swallowed\n");
        return 1;
    }
    printf("OK: extern struct ptr chain return works\n");
    return 0;
}
