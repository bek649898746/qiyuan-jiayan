// @EXPECTED compile_fail
#include <stdio.h>
/* P0-3 E4 (2026-09-10): 英文 struct + 中文借用 + 按值传参 → A3004
   修复前: 参移动 判据 甲言模式(语法关键词) 漏 英文 struct + 借用(词变量) 场景 → 借用期 move 漏拦 */
struct Inner { char *p; int v; };
int h(struct Inner x) { return x.v; }
函数 整 主(空) {
    struct Inner y; y.p = 0; y.v = 5;
    借用 y;
    struct Inner *r = &y;
    int t = h(y);        /* 借用期按值 move → A3004 */
    放还 y;
    返 t == 5 ? 0 : 1;
}
