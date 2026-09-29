# 包含 <stdio.h>
/* 字段名 撞型词 · 联合 路径 负例 (2026-09-29 批次 B t5 收口 R538):
   `联 U { 整 宽; 整 长; }` —— 保留型词 `长` 在声明侧被吞 (整 之后又是型词) ⟹ A9001 干净拒.
   同族覆盖: 具名顶层 (feat_zh_fieldkw_long_neg) · 嵌套内联 (feat_zh_field_decl_swallow_nested_neg)
   · 逗号续行 (feat_zh_fieldkw_comma_neg) · 本件 联合. */
// @EXPECTED compile_fail
// @EXPECTED err:声明侧吞字段
联 U { 整 宽; 整 长; }
函数 整 主(空) {
    联 U u; u.宽 = 1;
    格式化 "%d\n", u.宽;
    返 0;
}
