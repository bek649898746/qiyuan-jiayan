// @EXPECTED compile_fail
// @EXPECTED err:A3107
# 包含 <stdio.h>
/* 所有权 G1 (2026-09-09): 动态下标 (运行期索引) 整元素 不可移出 — A3107
   Rust E0508 精神: 运行期索引无法静态登记 → 移后读漏拦 (曾 aud2c 位拷贝无声放行)
   @EXPECTED compile_fail */
构 Y { 整 *p; }
函数 整 主(空) {
    构 Y 项[3]; 整 v0=1,v1=2,v2=3; 项[0].p=&v0; 项[1].p=&v1; 项[2].p=&v2;
    整 i = 1;
    构 Y t;
    移入 t, 项[i];               /* 运行期索引 整元素移出 → A3107 */
    若 (t.p != &v1) { 格式化 "FAIL\n"; 返 1; }
    返 0;
}
