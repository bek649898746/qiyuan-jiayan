// @EXPECTED compile_fail
/* 锚: 净缺口 #180「宏递归」—— C 6.10.3.4 p2（blue paint）：自引用宏**不再展开**。
   甲言 L804 显式防重入（`#定义 A(x) A(x)` 防死递归）⟹ 不挂死，而是落「未声明函数 SELF」响亮报错。
   契约「正确或响亮」：本锚 = 负锚（compile_fail）。
   ⚠️ 判据脚本 `scripts/check_macro180_selfref.py` 同时验证：编译**不超时**（未死循环）+ 正常宏对照 rc=0。 */
#define SELF(x) SELF(x)
函数 整 主(空) { 返 SELF(1); }
