// @EXPECTED out:PASS
#include <stdio.h>

/* 前向 typedef 指针成员 —— ORD-20261104-115 / ORD-20261103-98 / ORD-20261104-107 共同锚
 *
 * 病灶：`typedef struct Ctrl *Ctrlp;`（Ctrl 尚未定义）登记别名时丢了「指针身份」与「目标标签」：
 *   ① 是否指针标 = 0（指元素尺寸 记成 4）⟹ 成员 `Ctrlp opaque;` 只按 8 字节值登记，
 *      没标 原进/高贰 ⟹ `i.opaque->a` 把**成员地址**当值，解引用那一步被省掉；
 *   ② 指构序 = -1 且 构名一 = "" ⟹ 结构体后补定义时无人回填 ⟹ 字段毛利 = -1 ⟹
 *      `->a` / `->b` 字段偏移同为 0（两者读出同一个值）。
 *
 * 修法（单点链）：指针别名登记时带上基结构体标签名；`实重一(结构体定义完成)` 回填
 *   型定表.指构序 与 已登记成员的 毛利/符肆/主挂；typedef 成员分支补 指针身份。
 *
 * 灵敏度（双向钉死）：修复前 `A 821030512 821030512`（等于 &i.opaque）、`P3 1`；
 *   修复后 `A 7 8`、`P3 0`。对照组（先定义后 typedef）必须始终 `BG 17 18 4`。
 */

typedef struct Ctrl *Ctrlp;                       /* 前向：struct Ctrl 未定义 */
typedef struct Img { Ctrlp opaque; int w; } Img;
struct Ctrl { int a; int b; };

struct BEle { int x; int y; };                    /* 对照组：先完整定义再 typedef */
typedef struct BEle *BElep;
typedef struct BImg { BElep p; int w; } BImg;

int main(void) {
    int bad = 0;
    struct Ctrl c; c.a = 7; c.b = 8;
    Img i; i.opaque = &c; i.w = 3;

    if (i.opaque->a != 7 || i.opaque->b != 8) { printf("FAIL A %d %d\n", i.opaque->a, i.opaque->b); bad++; }
    if ((*i.opaque).a != 7 || (*i.opaque).b != 8) { printf("FAIL B %d %d\n", (*i.opaque).a, (*i.opaque).b); bad++; }
    if (i.w != 3) { printf("FAIL W %d\n", i.w); bad++; }
    if (i.opaque->a == (int)(long)&i.opaque) { printf("FAIL P3 = member address\n"); bad++; }  /* 防「换成另一个地址值」误判 */

    struct Ctrl *raw = &c;
    if (raw->a != 7 || raw->b != 8) { printf("FAIL RAW %d %d\n", raw->a, raw->b); bad++; }

    struct BEle be; be.x = 17; be.y = 18;
    BImg bi; bi.p = &be; bi.w = 4;
    if (bi.p->x != 17 || bi.p->y != 18 || bi.w != 4) { printf("FAIL BG %d %d %d\n", bi.p->x, bi.p->y, bi.w); bad++; }

    if (bad) { printf("FAIL total=%d\n", bad); return 1; }
    printf("PASS\n");
    return 0;
}
