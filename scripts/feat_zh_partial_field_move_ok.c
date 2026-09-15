// @EXPECTED out:PARTIAL FIELD OK
# 包含 <stdio.h>
/* P3-1 正例锚 (2026-09-10): 部分移出 只影响被移字段 — 其余字段 读写 放行; 重新赋值 整体恢复
   @EXPECT out:PARTIAL FIELD OK */
构 Y { 字 *p; 整 v; };
函数 整 主(空) {
    构 Y a; a.p = 0; a.v = 3;
    字 *q; 移入 q, a.p;          /* 部分移出 p */
    若 (a.v != 3) { 格式化 "FAIL 未移字段 v=%d\n", a.v; 返 1; }
    a.v = 4;                     /* 未移字段 可写 */
    若 (a.v != 4) { 格式化 "FAIL 写后 v=%d\n", a.v; 返 2; }
    构 Y b; b.p = 0; b.v = 9;
    移入 a, b;                   /* 整体重新赋值 → 恢复 (部分移动态清除) */
    若 (a.v != 9) { 格式化 "FAIL 恢复后 v=%d\n", a.v; 返 3; }
    格式化 "PARTIAL FIELD OK\n", 0;
    返 0;
}
