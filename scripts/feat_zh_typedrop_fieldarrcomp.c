// @EXPECTED out:TD B2C OK
# 包含 <stdio.h>
/* TypeDrop B2 复合数组字段探针: 合成容器数组字段 逐元素递归析构
   { 构 Y y = 造Y(); } → 计数=3 (y.zs[0..2].x 各析构一次)
   @EXPECT out:TD B2C OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
空 析构X(构 X *x) { 计数 = 计数 + 1; }
析构 X, 析构X;
构 Z { X x; }
构 Y { Z zs[3]; }
构 Y 造Y() { 构 Y y; y.zs[0].x.tag = 0; y.zs[1].x.tag = 1; y.zs[2].x.tag = 2; 返 y; }
函数 整 主(空) {
    { 构 Y y = 造Y(); }
    若 (计数 != 3) { 格式化 "FAIL 计数=%d 期望3\n", 计数; 返 1; }
    格式化 "TD B2C OK\n", 0;
    返 0;
}
