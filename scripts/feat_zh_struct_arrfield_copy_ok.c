// @EXPECTED out:ARRFIELD OK
# 包含 <stdio.h>
/* P2-20 (2026-09-10 修 en 链 regress_static_struct_assign): 构值 含数组字段 时, 数组字段不计入"需全写"字段
   — 语料常态是 逐元素填数组 (s.v[0]=7) + 整体拷贝; 计入会把这种写法误判 部分初始化 → A4001 假阳
   @EXPECT out:ARRFIELD OK */
构 S { 字 名[64]; 双 d; 整 v[3]; };
函数 整 主(空) {
    构 S s; s.d = 3.5; s.v[0] = 7; s.v[1] = 8; s.v[2] = 9;
    构 S t; t = s;                 /* 整体拷贝: 名[] 未写 不再误拦 */
    若 (t.d != 3.5 || t.v[2] != 9) { 格式化 "FAIL %f %d\n", t.d, t.v[2]; 返 1; }
    格式化 "ARRFIELD OK\n", 0;
    返 0;
}
