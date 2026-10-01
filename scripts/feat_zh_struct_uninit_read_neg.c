// @EXPECTED compile_fail
// @EXPECTED err:A4001
#include <stdio.h>
/* P1-7 I2 (2026-09-10): 结构体局部变量未初始化读字段 → A4001
   修复前: 构 X t; t.tag 放行 (变构 未登记尾进) */
构 X { 字 *p; 整 tag; }
函数 整 主(空) {
    构 X t;
    格式化 "%d\n", t.tag;   /* 读未初始化字段 → A4001 */
    返 0;
}
