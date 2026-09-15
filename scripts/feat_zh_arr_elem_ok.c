// @EXPECTED out:ARR ELEM OK
// @EXPECT out:ARR ELEM OK
# 包含 <stdio.h>
/* 2026-09-10 I2-③ 元素级 正例:
   ① 已 touched 元素 (常量下标写) 读 → 放行
   ② 动态下标写 (循环逐个初始化, 语料常见) → 降 不追踪, 后续常量下标读 不误拦 */
构 P { 整 x; 整 y; }
函数 整 主(空) {
    构 P a[3];
    a[0].x = 1; a[0].y = 2;
    若 (a[0].x + a[0].y != 3) { 格式化 "FAIL elem read\n", 0; 返 1; }
    遍 (整 i = 0; i < 3; i++) { a[i].x = i; a[i].y = i; }   /* 动态下标 → 不追踪 */
    若 (a[2].y != 2) { 格式化 "FAIL dyn init\n", 0; 返 2; }
    格式化 "ARR ELEM OK\n", 0;
    返 0;
}
