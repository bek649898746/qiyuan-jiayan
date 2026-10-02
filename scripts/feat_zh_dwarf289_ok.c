// @EXPECTED out:DW289 甲=42 乙=42
// @EXPECTED exit:0
// 族15 #289 调试信息 (DWARF): -g 产物的行号/文件归属 必须 指向 真源。
// 本锚 是 scripts/check_dwarf289.py 的行号基准 —— 检查器 现场 反查 本文件 的函数定义行,
// 与 .debug_info 的 decl_line / .debug_line 的行号程序 对账; 运行期断言 只验 程序本身。
# 包含 <stdio.h>

函数 整 甲(整 x) {
    返 x + 1;
}

函数 整 乙(整 x) {
    返 x * 2;
}

函数 整 主(空) {
    格式化 "DW289 甲=%d 乙=%d\n", 甲(41), 乙(21);
    返 0;
}
