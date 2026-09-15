#包含 <stdio.h>

/* C23 #elifdef / #elifndef 预处理简写 (等价 #elif defined(名) / #elif !defined(名)) */

// @EXPECTED out:elifdef/elifndef PASS
#define 甲 1

函数 整 主(空) {
#ifdef 甲
    整 v = 1;
#elifdef 不存在宏
    整 v = 2;
#else
    整 v = 3;
#endif

    /* ① #ifdef 甲 真 → v=1 (elifdef 不触发) */
    若 (v != 1) 返 1;

#ifndef 甲
    整 w = 10;
#elifndef 乙
    整 w = 20;
#else
    整 w = 30;
#endif

    /* ② #ifndef 甲 假 → elifndef 检查: #elifndef 乙 → 乙未定义 → 真 → w=20 */
    若 (w != 20) 返 2;

    /* ③ elifdef 真分支: #ifdef 不存在宏 假 → #elifdef 甲 → 真 → u=5 */
#ifdef 不存在宏
    整 u = 1;
#elifdef 甲
    整 u = 5;
#else
    整 u = 9;
#endif
    若 (u != 5) 返 3;

    /* ④ 嵌套 + else 兜底: #ifdef 不存在 → #elifdef 乙 (未定义) → #else → t=7 */
#ifdef 不存在宏
    整 t = 1;
#elifdef 乙
    整 t = 2;
#else
    整 t = 7;
#endif
    若 (t != 7) 返 4;

    格式化 "elifdef/elifndef PASS\n";
    返 0;
}
