// @EXPECTED out:ENUMQ 3 2 2 7
// @EXPECTED exit:0
/* 锚: `const/static enum T v;` 局部声明族（R905f · 审计 ORD-33 修点①）
   修前（4EEE80A1）：`const enum Color c = ...;` / `static const enum ...` 报 A1004 —— 声明分支的 总列一
   按**首词元**取值，首词元是 const/static ⟹ 枚举三块全跳过，型词循环后词位停在 `enum` 无人消费。
   现场：git 2.45.2 `dir.h` L662/L674。覆盖 ① const enum 初始化 ② static const enum ③ const enum 形参
   ④ enum T * 指针声明。（`(enum T)`/`(enum T *)` **转型** 仍未支持，另挂 ORD-20261029-42。） */
#包含 <stdio.h>

枚 色 { 红 = 1, 绿 = 2, 蓝 = 3 };

静 枚 色 取(整 i) {
    枚 色 v = 红;
    若 (i >= 1) v = 绿;
    返 v;
}

函数 整 取形参(常 枚 色 c) {
    返 (整)c;
}

函数 整 主(空) {
    常 枚 色 c = 蓝;          /* ① */
    静 常 枚 色 s = 绿;       /* ② */
    枚 色 格 = 红;
    枚 色 *p = &格;           /* ④ */
    格式化 "ENUMQ %d %d %d %d\n", (整)c, (整)s, 取(1), (整)(*p) + 6;
    返 取形参(c) - 3;
}
