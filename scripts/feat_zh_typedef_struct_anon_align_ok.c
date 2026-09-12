// @EXPECTED out:anon struct sz=8 tsz=24
#include <stdio.h>
/* 形态锚 + 布局断言 (启元 2026-09-13 复盘 序5/序6): **匿名 typedef struct 尾部对齐填充**。
   该形态 是 BUG-2 所在格 —— 复盘时 61 个锚命中此形态, 但 **只有 5.7% 的锚验 sizeof**, 所以缺陷长期潜伏。
   本锚 钉: 尺寸 必须按最大对齐 **轮 up** (原缺 → 小数尺寸)。
   gcc 基准: {int;char[2]} = 8; {int;char[4];double;int;char[2]} = 24 */
typedef 构 { 整 a; 字 b[2]; } TE;
typedef 构 { 整 bx; 字 by[4]; 双 bz; 整 dx; 字 dy[2]; } TA;
函数 整 主(空) {
    若 (大小(TE) != 8) { 格式化 "FAIL TE=%d\n", 大小(TE); 返 1; }
    若 (大小(TA) != 24) { 格式化 "FAIL TA=%d\n", 大小(TA); 返 2; }
    TE 阵[2];                /* 数组步长 == sizeof (stride 同错 曾 一并 覆盖) */
    阵[0].a = 7; 阵[1].a = 9;
    若 (阵[0].a != 7 || 阵[1].a != 9) { 格式化 "FAIL 步长\n"; 返 3; }
    格式化 "anon struct sz=%d tsz=%d\n", 大小(TE), 大小(TA);
    返 0;
}
