// @EXPECTED exit:0
# 包含 <stdio.h>
/* G2b (2026-09-10): 大结构体 (>8B) 错误载荷 — 结构体 走 sret (rcx 隐藏指针), rax = tag, rdx = 错码
   消费侧 临时 用 构 槽 (sret 直写), tag 经 __qcc_rax 捕获; 传播 经 尝试 + 错码透传 (rdx)
   (改前: A6004 拒绝 "结构体值载荷暂不支持 (无法打包进 64 位 rax/rdx)") */
构 大 { 整 a; 整 b; 整 c; 整 d; }
错误 构 大 取(整 c) { 若 (c < 0) 错 7; 构 大 p; p.a = 1; p.b = 2; p.c = 3; p.d = 4; 返 p; }
错误 构 大 外(整 c) { 尝试 取(c); 构 大 q; q.a = 9; q.b = 9; q.c = 9; q.d = 9; 返 q; }
函数 整 主(空) {
    构 大 v; 整 t = -1;
    解构(v, t) = 取(1);
    若 (t != 0 || v.a != 1 || v.b != 2 || v.c != 3 || v.d != 4) { 格式化 "FAIL ok %d %d %d\n", t, v.a, v.d; 返 1; }
    构 大 w; 整 tw = -1;
    解构(w, tw) = 取(-1);
    若 (tw != 1) { 格式化 "FAIL err-tag %d\n", tw; 返 2; }
    构 大 z; 整 tz = -1;
    解构(z, tz) = 外(-1); /* 3 层: 叶 错 7 → 外 尝试 传播 */
    若 (tz != 1) { 格式化 "FAIL propagate %d\n", tz; 返 3; }
    构 大 y; 整 ty = -1;
    解构(y, ty) = 外(5);
    若 (ty != 0 || y.a != 9 || y.d != 9) { 格式化 "FAIL ok2 %d %d\n", ty, y.a; 返 4; }
    格式化 "ERR STRUCT BIG OK\n", 0;
    返 0;
}
