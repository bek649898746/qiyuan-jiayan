// @EXPECTED out:STRUCT ARR ELEM OK
// @EXPECTED exit:0
# 包含 <stdio.h>
/* ★ R314 (2026-09-26 启元 · 第 364 站 round 44): **补锚** —— 数组元素赋值的「防过严」守卫锚 (R313 判据)。
 * 同类型 元素赋值 / 元素互赋 必须放行, 且运行值正确 —— 覆盖 R313 新逻辑的**正向**面。 */
构 元K { 整 a; };
构 元K2 { 整 a; 整 b; };            /* 大一点的结构体 (8B) */

函数 整 主(空) {
    构 元K arr[2];
    构 元K s; s.a = 44;
    arr[0] = s;                              /* 同类型 元素赋值 → 放行 */
    若 (arr[0].a != 44) { 格式化 "FAIL ① %d\n", arr[0].a; 返 1; }
    arr[1] = arr[0];                         /* 元素互赋 同类型 */
    若 (arr[1].a != 44) { 格式化 "FAIL ② %d\n", arr[1].a; 返 2; }
    构 元K2 big[1];
    构 元K2 b; b.a = 5; b.b = 6;
    big[0] = b;                              /* 8B 元素 同类型 */
    若 (big[0].a != 5 || big[0].b != 6) { 格式化 "FAIL ③ %d %d\n", big[0].a, big[0].b; 返 3; }
    格式化 "STRUCT ARR ELEM OK\n", 0;
    返 0;
}
