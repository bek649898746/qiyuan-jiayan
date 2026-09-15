// @EXPECTED out:ERR STRUCT SMALL OK
// @EXPECTED exit:0
# 包含 <stdio.h>
/* G2a (2026-09-10): 小结构体 (≤8B) 错误载荷 — 结构体 ≤8B 按值返回, 值 占 rax, tag 走 rdx (与 长整 同类)
   检查形态 解构(v, t) = f(); 传播形态 错误函数 内 尝试 f();
   (改前: A6004 拒绝 "结构体值载荷暂不支持") */
构 点 { 整 x; 整 y; }
错误 构 点 取(整 c) { 若 (c < 0) 错 7; 构 点 p; p.x = 1; p.y = 2; 返 p; }
错误 构 点 外(整 c) { 尝试 取(c); 构 点 q; q.x = 9; q.y = 9; 返 q; }
函数 整 主(空) {
    构 点 a; 整 ta = -1;
    解构(a, ta) = 取(1);
    若 (ta != 0 || a.x != 1 || a.y != 2) { 格式化 "FAIL ok %d %d %d\n", ta, a.x, a.y; 返 1; }
    构 点 b; 整 tb = -1;
    解构(b, tb) = 取(-1);
    若 (tb != 1) { 格式化 "FAIL err-tag %d\n", tb; 返 2; }
    构 点 c2; 整 tc = -1;
    解构(c2, tc) = 外(-1); /* 传播: 叶 错 7 → 外 尝试 → tag 1 */
    若 (tc != 1) { 格式化 "FAIL propagate %d\n", tc; 返 3; }
    构 点 d; 整 td = -1;
    解构(d, td) = 外(5); /* 成功路径: 外 自身 构造 的 结构体 */
    若 (td != 0 || d.x != 9 || d.y != 9) { 格式化 "FAIL ok2 %d %d\n", td, d.x; 返 4; }
    格式化 "ERR STRUCT SMALL OK\n", 0;
    返 0;
}
