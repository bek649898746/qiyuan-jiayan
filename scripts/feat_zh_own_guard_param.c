# 包含 <stdio.h>
/* 所有权 守卫形参 传参移动 (2026-09-06 虾米 BatchB2): f(b) 传参移动 b (非Copy) → b F=0; f 内 形参守卫 → f 结束析构 1 次
   main b 不析构 → 总计数 1 (旧: main 陈旧F 双析构 计数2 — 语句级 F 冲刷修复)
   @EXPECT out:FEAT OWN PARAMG OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
整 f(构 X r) {
    守卫 r, 析构X;
    若 (r.tag != 7) { 格式化 "FAIL tag=%d\n", r.tag; 返 1; }
    返 0;
}
函数 整 主(空) {
    { 构 X b = 新X(7);
      守卫 b, 析构X;
      整 r = f(b);
      若 (r != 0) 返 1;
    }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d 期望1\n", 计数; 返 1; }
    格式化 "FEAT OWN PARAMG OK\n", 0;
    返 0;
}
