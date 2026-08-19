/* 复现: sizeof(*base->member) 成员链 deref 大小 —
   * 简单: sizeof(*p) → sizeof(struct)
   * 链: sizeof(*dir->entries) → 8 (entries 是 ** 指针数组)
   @EXPECTED
   exit: 0
   out: elemsz=8 structsz=16
   */
#include <stdio.h>

struct ref_entry;

struct ref_dir {
    int nr, alloc, sorted;
    void *cache;
    struct ref_entry **entries;
};

struct simple {
    int a;
    int b;
};

int main(void)
{
    struct ref_dir *dir = 0;
    struct simple *sp = 0;
    int elemsz = (int)sizeof(*dir->entries);
    int structsz = (int)sizeof(*sp);
    printf("elemsz=%d structsz=%d\n", elemsz, structsz);
    if (elemsz == 8 && structsz == 8) return 0;
    return 1;
}
