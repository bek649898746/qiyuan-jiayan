/* 复现: git hash-ll.h 的嵌套宏数组维度 —
   #define GIT_SHA1_RAWSZ 20
   #define GIT_SHA256_RAWSZ 32
   #define GIT_MAX_RAWSZ GIT_SHA256_RAWSZ
   struct object_id { unsigned char hash[GIT_MAX_RAWSZ]; int algo; };
   若嵌套宏未展开 → hash[GIT_SHA256_RAWSZ] 未解析 → 数组尺寸错 →
   sizeof(object_id) != 36 → ref_entry 布局错位。
   @EXPECTED
   exit: 0
   out: oid_size=36 oid_algo=32 max=32
   (fix 2026-08-20 启元: 期望 40→36 — struct{char[32];int} C 标准 = 36, gcc 验证同;
    原测试期望 40 是错误期望, 非编译器 bug)
   */
#include <stdio.h>

#define GIT_SHA1_RAWSZ 20
#define GIT_SHA256_RAWSZ 32
#define GIT_MAX_RAWSZ GIT_SHA256_RAWSZ

struct object_id {
    unsigned char hash[GIT_MAX_RAWSZ];
    int algo;
};

int main(void)
{
    printf("oid_size=%d oid_algo=%d max=%d\n",
           (int)sizeof(struct object_id),
           (int)((char *)&((struct object_id *)0)->algo - (char *)0),
           (int)GIT_MAX_RAWSZ);
    if (sizeof(struct object_id) == 36) return 0;
    return 1;
}
