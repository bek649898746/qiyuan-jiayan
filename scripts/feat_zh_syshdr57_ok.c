// @EXPECTED out:SYS57 15 260 20
/* ORD-57 (2026-10-30 虾米): **内置头 / 内建名 缺口** —— 甲言跳过 `<...>` 系统头 ⟹ 其宏/声明不可见。
   修前：`char buff[L_tmpnam]` ⟹ A1001 'L_tmpnam'（lua `loslib.c` 的 `os_tmpname`）；
        `LC_ALL` 六常量 ⟹ A1001 'LC_ALL'（`os_setlocale`）；`setlocale(...)` ⟹ A1001 未声明函数。
   修法：内置头宏区补 `<stdio.h>` 三常量 + `<locale.h>` 六常量（**值取 mingw-w64 实测**），
        内建名表 `主薄` 补 `setlocale`。
   本锚真跑：三个常量族都可用。 */
#包含 <stdio.h>
#包含 <locale.h>

函数 整 主(空) {
    字 缓冲[L_tmpnam];
    整 和 = LC_ALL + LC_COLLATE + LC_CTYPE + LC_MONETARY + LC_NUMERIC + LC_TIME;
    缓冲[0] = 0;
    格式化 "SYS57 %d %d %d\n", 和, (整)L_tmpnam, (整)FOPEN_MAX;
    返 (和 == 15 && (整)L_tmpnam == 260) ? 0 : 1;
}
