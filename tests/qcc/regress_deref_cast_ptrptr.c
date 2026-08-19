/* 复现: *(cast)ptr 解引用宽度 —
   git ref_entry_cmp_sslice: ent = *(const struct ref_entry * const *)ent_;
   ent_ 是 const void * (8B 指针), cast 后 deref 得 struct 指针 (8B)。
   若 qcc 把 deref 宽度算成 1 (movzbl) → ent 只有低字节。
   @EXPECTED
   exit: 0
   out: ent_ok=1
   */
#include <stdio.h>

struct entry {
    char name[16];
};

static int deref_ok(const void *ent_)
{
    /* 解引用后应是完整 8 字节指针 */
    const struct entry *ent = *(const struct entry * const *)ent_;
    if ((unsigned long)ent == 0x12345678ULL) return 1;
    return 0;
}

int main(void)
{
    unsigned long fake = 0x12345678ULL;
    int ok = deref_ok(&fake);
    printf("ent_ok=%d\n", ok);
    if (ok) return 0;
    return 1;
}
