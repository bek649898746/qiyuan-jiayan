// @EXPECTED out:FEAT OWN DROPROK OK
# 包含 <stdio.h>
/* 所有权 丢弃后重赋值 (2026-09-06 虾米 G6+BatchB): 守卫 b; 丢弃 b (析构1+F=0); b=新X(2) (重武装) → 作用域退出析构 = 2
   旧: 槽注销后 新值漏析构 (计数1) 或 覆写误析构陈旧槽 (计数3)
   @EXPECT out:FEAT OWN DROPROK OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    { 构 X b = 新X(1);
      守卫 b, 析构X;
      丢弃 b;
      b = 新X(2);
    }
    若 (计数 != 2) { 格式化 "FAIL 计数=%d 期望2\n", 计数; 返 1; }
    格式化 "FEAT OWN DROPROK OK\n", 0;
    返 0;
}
