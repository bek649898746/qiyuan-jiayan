// @EXPECTED out:PARTIAL REINIT OK
// @EXPECT out:PARTIAL REINIT OK
# 包含 <stdio.h>
/* I2-③ (2026-09-10) 正例: 污染态 整体赋值 → 重初始化恢复, 后续读 OK */
构 P { 整 a; 整 b; }
构 P 造P(整 a, 整 b) { 构 P p; p.a = a; p.b = b; 返 p; }  /* 构造器在 主 前 (序集=0 不查) */
函数 整 主(空) {
    构 P s;              /* 未初始化登记 */
    s.a = 5;             /* 字段写 → 污染态 */
    s = 造P(3, 4);       /* 整体赋值 (节点_变量 分支) → 清污染/重初始化 */
    若 (s.b != 4) { 格式化 "FAIL b=%d 期望4\n", s.b; 返 1; }
    若 (s.a != 3) { 格式化 "FAIL a=%d 期望3\n", s.a; 返 1; }
    格式化 "PARTIAL REINIT OK\n", 0;
    返 0;
}
