// @EXPECTED exit:0
// @EXPECTED out:OK
/* R1-B 锤A (2026-09-04) #97 判别 类型化载荷 真 sum type 布局:
   判别 Shape 于 { 圆(双 半径), 方(整 宽, 整 高), 点() } → tag@0 + 载荷联合式重叠
   (圆 半径 双@8; 方 宽@8 高@12 与 半径 重叠; sizeof=16) — 无 p0/p1 混用, 各按真类型存取.
   旧式 (未类型载荷) 判别 保持 tag/p0/p1 原布局回归. */
#include <stdio.h>

int main(void) {
    判别 Shape 于 { 圆(双 半径), 方(整 宽, 整 高), 点() };
    判别 Old 于 { 甲(值), 乙() };

    /* 布局: tag@0, 双 对齐 → 载荷区基=8, 双@8..15, sizeof=16 (类型化) */
    struct Shape c;
    if ((int)sizeof(struct Shape) != 16) { printf("FAIL sizeof %d\n", (int)sizeof(struct Shape)); return 1; }
    if ((char*)&c.半径 - (char*)&c != 8) { printf("FAIL 半径 off %d\n", (int)((char*)&c.半径 - (char*)&c)); return 1; }
    if ((char*)&c.宽 - (char*)&c != 8) { printf("FAIL 宽 off %d\n", (int)((char*)&c.宽 - (char*)&c)); return 1; }
    if ((char*)&c.高 - (char*)&c != 12) { printf("FAIL 高 off %d\n", (int)((char*)&c.高 - (char*)&c)); return 1; }

    /* 圆: 双 载荷真存取 */
    c.tag = Shape_圆;
    c.半径 = 3.5;
    if (c.tag != Shape_圆) { printf("FAIL tag\n"); return 1; }
    if (c.半径 < 3.49 || c.半径 > 3.51) { printf("FAIL 半径 val %g\n", c.半径); return 1; }

    /* 方: 双载荷区重叠复用 (整 宽/高) */
    struct Shape d;
    d.tag = Shape_方;
    d.宽 = 4;
    d.高 = 6;
    if (d.宽 * d.高 != 24) { printf("FAIL 方 %d*%d\n", d.宽, d.高); return 1; }

    /* 点: 空载荷变体 */
    struct Shape e;
    e.tag = Shape_点;
    if (e.tag != 2) { printf("FAIL 点 tag %d\n", e.tag); return 1; }

    /* 依据 穷尽 判别值分发仍工作 (tag 整 匹配) */
    int r = 0;
    依据 r, d.tag 穷尽 Shape 对照 Shape_圆: 1, Shape_方: 2, Shape_点: 3;
    if (r != 2) { printf("FAIL 依据 %d\n", r); return 1; }

    /* 旧式 (未类型载荷) 判别 回归: tag/p0/p1 原布局 */
    struct Old o;
    o.tag = Old_甲;
    o.p0 = 7;
    if (o.p0 != 7) { printf("FAIL old p0\n"); return 1; }
    依据 r, o.tag 穷尽 Old 对照 Old_甲: 11, Old_乙: 22;
    if (r != 11) { printf("FAIL old 依据 %d\n", r); return 1; }

    printf("OK\n");
    return 0;
}
