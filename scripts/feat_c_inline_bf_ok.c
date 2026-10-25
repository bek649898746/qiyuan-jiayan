// @EXPECTED out:A=3412 B=4 C=8 D=4
/* A71-续10 (2026-09-16): 内联匿名结构体 的 位域 —— 族 D 根治锚。
   病灶: 内联匿名结构体的字段循环 (两条路径: `深进` = 类定义处内联体 / `行散` = 嵌套结构体成员的内联体)
         对位域 **只** 做 `新升 = 4` (注释「位域: 32 位槽」= 设计假设: 不打包),
         随后 **照旧走普通字段登记** (字段尺寸递归 / 联合字段) ⟹ 每个位域各占 4 字节。
         实测: `union U { struct { unsigned a:8; unsigned b:8; } f; };` → 尺寸 8 (应 4);
               `u.f.a=0x12; u.f.b=0x34;` → w=0x12 (应 0x3412);
         而 **具名** 结构体作成员正确、**独立** 结构体位域也正确
         ⟹ 只差「内联匿名体 + 位域」这一格 (插桩实证: 字段 bw=0、off=0/4)。
   根治: 位域改走 `构域位` (镜像独立结构体路径) + 补 `无符D`/`无符D2` (原把 `unsigned` 二字直接丢弃)。
   本锚钉: 联合内联位域 / 嵌套结构体成员内联位域 / 双层嵌套 / 联合尺寸, 与 gcc 逐字节一致。 */
#include <stdio.h>

union U { struct { unsigned a:8; unsigned b:8; } f; unsigned short w; };
struct Outer { struct { unsigned a:8; unsigned b:8; } inner; unsigned short w; };
struct A { struct { struct { unsigned a:4; unsigned b:4; } deep; } mid; };

int main(void) {
    union U u;
    u.w = 0;
    u.f.a = 0x12;
    u.f.b = 0x34;
    printf("A=%x B=%d C=%d D=%d\n", u.w, (int)sizeof(union U), (int)sizeof(struct Outer), (int)sizeof(struct A));
    return 0;
}
/* @EXPECTED out:A=3412 B=4 C=8 D=4 */
