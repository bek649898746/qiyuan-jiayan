# 包含 <stdio.h>
/* 所有权 drop 环 (2026-09-05 虾米 ring3a): 守卫变量被移动 → 源守卫注销 (作用域退出不得二次析构同值; 防双 free) — 非Copy 真移动: 移动后仅目标析构一次 */
// @EXPECTED out:OWN GUARD MOVE PASS
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    {
        构 X a = 新X(1);
        守卫 a, 析构X;
        构 X b = 新X(2);
        守卫 b, 析构X;
        b = a;              /* a 真移动给 b (非Copy): a 守卫注销 */
        b.tag = 5;
    }
    若 (计数 != 1) 返 1;    /* 仅新 b 析构一次; a 移出不析构 (旧 b 覆写析构 = 待办) */
    格式化 "OWN GUARD MOVE PASS\n", 0;
    返 0;
}
