// @EXPECTED out:PASS
#include <stdio.h>

/* R232 补锚 — **成员读「五路径」× 指针字段（所指结构体恰 8 字节）**
 *
 * 病根族（与 R231 同）：成员读各处判据用「字段类型序指向的结构体尺寸」**间接猜**指针性，
 *   而未用**权威标记** `字段是否指`（即 `类型表[构序].字段[].原进 != 0`）。
 *   所指向的结构体恰好 8 字节时判据为假 ⟹ 落「由-值 结构体 → 保留地址」⟹ 读出 &宿主+偏移（字段地址）。
 *   R231 修「静态 点成员」；R232 修「箭头」两个分支（`变量型序(基) >= 0` 与兜底猜类型）。
 *
 * 本锚一次钉住五条路径 × 两种登记形态（具名 struct Ho / typedef 匿名 Anon）：
 *   ① 静态点成员 NS.p 与 AS.p
 *   ② 静态箭头 NPS->p 与 APS->p        ← R232 靶心（改前读出 &宿主+8）
 *   ③ 静态数组元素 NAR[0].p 与 AAR[0].p
 *   ④ 局部点成员 NL.p 与 AL.p
 *   ⑤ 局部箭头 NPL->p 与 APL->p        ← R232 靶心（改前读出 &NL+8）
 * 「所指结构体恰 8 字节」是关键 —— struct In { int x; char c; } 尺寸 8，正是歧义尺寸。
 */

struct In { int x; char c; };
struct Ho { int nr; struct In *p; };

static struct Ho NS;              /* 具名: 静态点成员 / 箭头 / 数组 */
static struct Ho *NPS;
static struct Ho NAR[2];

typedef struct { int nr; struct In *p; } Anon;   /* 匿名(tree) 经 typedef —— 另一条登记路径 */
static Anon AS;
static Anon *APS;
static Anon AAR[2];

int main(void) {
    struct Ho NL; struct Ho *NPL;
    Anon AL;     Anon *APL;

    NS.nr = 1; NS.p = (void *)0x11;  NPS = &NS;
    NL.nr = 2; NL.p = (void *)0x12;  NPL = &NL;
    NAR[0].nr = 3; NAR[0].p = (void *)0x13;

    AS.nr = 4; AS.p = (void *)0x14;  APS = &AS;
    AL.nr = 5; AL.p = (void *)0x15;  APL = &AL;
    AAR[0].nr = 6; AAR[0].p = (void *)0x16;

    if ((void *)NS.p != (void *)0x11) return 1;      /* ① 具名 静态点成员 */
    if ((void *)NPS->p != (void *)0x11) return 2;    /* ② 具名 静态箭头  ← R232 */
    if ((void *)NAR[0].p != (void *)0x13) return 3;  /* ③ 具名 静态数组元素 */
    if ((void *)NL.p != (void *)0x12) return 4;      /* ④ 具名 局部点成员 */
    if ((void *)NPL->p != (void *)0x12) return 5;    /* ⑤ 具名 局部箭头  ← R232 */
    if ((void *)AS.p != (void *)0x14) return 6;      /* ① 匿名 静态点成员 */
    if ((void *)APS->p != (void *)0x14) return 7;    /* ② 匿名 静态箭头  ← R232 */
    if ((void *)AAR[0].p != (void *)0x16) return 8;  /* ③ 匿名 静态数组元素 */
    if ((void *)AL.p != (void *)0x15) return 9;      /* ④ 匿名 局部点成员 */
    if ((void *)APL->p != (void *)0x15) return 10;   /* ⑤ 匿名 局部箭头  ← R232 */

    printf("PASS\n");
    return 0;
}
