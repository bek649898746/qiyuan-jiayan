// @EXPECTED exit:0
// @EXPECTED out:OK
/* R1-B 锤B (2026-09-04) #98 依据 判别 变体载荷绑定: 对照 圆(半径): 结果 — 真解构
   (消费 荷布/变载/判载名: 匹配判别值 + 臂内绑定 X.载荷字段 至 类型化局部变量) */
#include <stdio.h>

int main(void) {
    判别 Shape 于 { 圆(双 半径), 方(整 宽, 整 高), 点() };

    /* A: 双 载荷 绑定 + 双 结果变量 (3*2²=12) */
    struct Shape c;
    c.tag = Shape_圆; c.半径 = 2.0;
    双 area;
    依据 area, c.tag 穷尽 Shape 对照 圆(半径): 3.0 * 半径 * 半径, Shape_方: 2, 点(): 0, 其他: 0;
    if (area != 12.0) { printf("FAIL A %g\n", area); return 1; }

    /* B: 常量臂 (Shape_方) 匹配 双 结果 = -2.5 保真 (__sr 随结果变量类型) */
    struct Shape d;
    d.tag = Shape_方; d.宽 = 3; d.高 = 5;
    依据 area, d.tag 穷尽 Shape 对照 圆(半径): 1.0, Shape_方: -2.5, 点(): 9.0;
    if (area != -2.5) { printf("FAIL B %g\n", area); return 1; }

    /* C: 整 双载荷绑定 + 整 结果变量 (4*6=24) */
    struct Shape e;
    e.tag = Shape_方; e.宽 = 4; e.高 = 6;
    整 ar;
    依据 ar, e.tag 穷尽 Shape 对照 圆(半径): 0, 方(宽, 高): 宽 * 高, 点(): 0;
    if (ar != 24) { printf("FAIL C %d\n", ar); return 1; }

    /* D: 空载荷变体 点() + 缺省其他 */
    struct Shape f;
    f.tag = Shape_点;
    依据 area, f.tag 穷尽 Shape 对照 圆(半径): 1.5, Shape_方: 2.5, 点(): 3.5, 其他: 0;
    if (area != 3.5) { printf("FAIL D %g\n", area); return 1; }
    f.tag = 9; /* 无匹配判别值 → 其他 */
    依据 area, f.tag 穷尽 Shape 对照 圆(半径): 1.5, Shape_方: 2.5, 点(): 3.5, 其他: 7.25;
    if (area != 7.25) { printf("FAIL D2 %g\n", area); return 1; }

    /* E: 双 载荷 圆周率实算 (3.14159*5*5≈78.5397) */
    struct Shape g;
    g.tag = Shape_圆; g.半径 = 5.0;
    依据 area, g.tag 穷尽 Shape 对照 圆(半径): 3.14159 * 半径 * 半径, Shape_方: 0, 点(): 0;
    if (area < 78.5 || area > 78.6) { printf("FAIL E %g\n", area); return 1; }

    printf("OK\n");
    return 0;
}
