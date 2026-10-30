// @EXPECTED out:WIN59 1 1 260
/* ORD-59 (2026-10-30 虾米): **Windows 平台标识 预定义 + 内置 <windows.h> 最小子集**。
   修前：`#if defined(_WIN32)` 在甲言下 **判假**（gcc 判真）⟹ lua `lprefix.h` 的 `LUA_USE_WINDOWS`
   不生效 ⟹ ① `loadlib.c` 的 `setprogdir` 用 `<windows.h>` 的 `MAX_PATH`/`DWORD` 但头被跳过 ⟹
   定义失败 ⟹ 调用点 A1001；② `lmathlib.c` 的 `#if defined(Rand64)` 与 `#else` 两条都编译 ⟹
   `lu_int32` 先用后定义 ⟹ A1004。
   修法：内置宏补 `_WIN32=1`/`_WIN64=1` + 新增内置头 `<windows.h>` 最小子集（只补 lua 用到的符号）。
   本锚真跑：两个平台标识都为真，且 `MAX_PATH` 可求值。 */
#包含 <stdio.h>
#包含 <windows.h>

函数 整 主(空) {
    整 甲 = 0, 乙 = 0;
#if defined(_WIN32)
    甲 = 1;
#endif
#if defined(_WIN64)
    乙 = 1;
#endif
    格式化 "WIN59 %d %d %d\n", 甲, 乙, (整)MAX_PATH;
    返 (甲 == 1 && 乙 == 1 && (整)MAX_PATH == 260) ? 0 : 1;
}
