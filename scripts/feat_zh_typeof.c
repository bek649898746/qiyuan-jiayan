#包含 <stdio.h>

/* C23 typeof 类型查询 (中文 类型取 双认) — typeof(变量)/typeof(类型) 声明 */

函数 整 主(空) {
    /* ① typeof(变量): 复制变量类型 */
    整 x = 5;
    typeof(x) y = x;
    若 (y != 5) 返 1;

    /* ② typeof(类型): 显式类型 */
    typeof(整) z = 7;
    若 (z != 7) 返 2;

    /* ③ typeof(变量) double */
    双 d = 3.5;
    typeof(d) e = d;
    若 (e != 3.5) 返 3;

    /* ④ typeof 声明后赋值/使用 */
    typeof(x) w;
    w = 9;
    若 (w != 9) 返 4;

    /* ⑤ typeof 指针: typeof(指针变量) 保持指针 */
    整 *p = &x;
    typeof(p) q = p;
    若 (*q != 5) 返 5;

    /* ⑥ typeof 用于返回值声明? (基本: 声明语句) */
    typeof(x) a = 1;
    typeof(a) b = 2;
    若 (a + b != 3) 返 6;

    格式化 "typeof 类型查询 PASS\n";
    返 0;
}
