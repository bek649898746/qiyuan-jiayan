// @EXPECTED compile_fail
// @EXPECTED err:A9006
/* 第 455 站·第四十八轮 负锚: **const typedef 别名 ⟹ 变量 只读**（局部档）。
   形态: `typedef 整 常 CI; CI x = 5; x = 6;` ⟹ A9006（gcc: assignment of read-only variable）。
   修复前: rc=0 真改写（typedef 表 未记「常量」属性）。 */
typedef 整 常 CI;
函数 整 主(空) {
    CI x = 5;
    x = 6;
    返 x;
}
