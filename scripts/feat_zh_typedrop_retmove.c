# 包含 <stdio.h>
/* 类型级析构 TypeDrop 返回=移动 (2026-09-06): 取件() 内 b 类型析构但 返 b 移动 → 函数内不析构; 调用侧 t 自动析构 = 1
   @EXPECT out:TD RETMOVE OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
构 X 取件(整 n) {
    构 X b = 新X(n);
    返 b;
}
函数 整 主(空) {
    { 构 X t = 取件(7);
      若 (t.tag != 7) 返 1; }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d 期望1\n", 计数; 返 1; }
    格式化 "TD RETMOVE OK\n", 0;
    返 0;
}
