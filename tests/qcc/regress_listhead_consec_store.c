/* 复现: tempfile.h 的 INIT_LIST_HEAD — volatile struct 成员 + 连续字段赋值。
   第二条 list store 继承第一条的 +8 偏移 → next 字段未被写 (next=0)。
   @EXPECTED
   exit: 0
   out: next=1 prev=2 correct
   */
#include <stdio.h>
#include <stdlib.h>

struct volatile_list_head {
    volatile struct volatile_list_head *next;
    volatile struct volatile_list_head *prev;
};

typedef unsigned long long size_t_jy;

struct strbuf_jy {
    size_t_jy alloc;
    size_t_jy len;
    char *buf;
};

struct tempfile {
    volatile struct volatile_list_head list;   /* @0 */
    volatile int fd;                            /* @16 */
    void *volatile fp;                          /* @24 */
    volatile int owner;                         /* @32 */
    struct strbuf_jy filename;                  /* @40 */
    char *directory;                            /* @64 */
};

#define INIT_LIST_HEAD(ptr) \
    (ptr)->next = (ptr)->prev = (ptr)

static struct tempfile *new_tempfile(void)
{
    struct tempfile *tempfile = malloc(sizeof(struct tempfile));
    tempfile->fd = -1;
    tempfile->fp = 0;
    tempfile->owner = 0;
    INIT_LIST_HEAD(&tempfile->list);
    tempfile->directory = 0;
    return tempfile;
}

int main(void)
{
    struct tempfile *t = new_tempfile();
    if (!t) return 9;
    if (t->list.next != &t->list) {
        printf("NEXT BAD next=%p expect=%p\n", (void *)t->list.next, (void *)&t->list);
        free(t);
        return 1;
    }
    if (t->list.prev != &t->list) {
        printf("PREV BAD prev=%p expect=%p\n", (void *)t->list.prev, (void *)&t->list);
        free(t);
        return 2;
    }
    printf("next=1 prev=2 correct\n");
    free(t);
    return 0;
}
