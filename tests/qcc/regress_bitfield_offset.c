/* 复现: 位域 + CRITICAL_SECTION 结构体字段的偏移 —
   git raw_object_store: replace_map_initialized:1 位域后跟
   pthread_mutex_t (=CRITICAL_SECTION, 40B) 再跟 commit_graph。
   若结构体字段大小没算进 → commit_graph 偏移少 40 → 读到 mutex 垃圾 → SEGV。
   @EXPECTED
   exit: 0
   out: g=96
   */
#include <stdio.h>

struct critical_section {
    void *debug;              /* @0 */
    int lockcount;            /* @8 */
    int recursion;            /* @12 */
    void *thread;             /* @16 */
    void *semaphore;          /* @24 */
    unsigned long long spin;  /* @32 */
};                            /* 40 bytes */

struct store {
    void *a;        /* @0 */
    void *b;        /* @8 */
    void *c;        /* @0x10 */
    int d;          /* @0x18 */
    void *e;        /* @0x20 */
    void *f;        /* @0x28 */
    unsigned bit:1; /* @0x30 位域 */
    struct critical_section m;  /* @0x38 mutex 40B */
    void *g;        /* @0x60 commit_graph */
};

int main(void)
{
    int off = (int)((char *)&((struct store *)0)->g - (char *)0);
    printf("g=%d\n", off);
    if (off == 96) return 0;
    return 1;
}
