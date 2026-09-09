# 包含 <stdio.h>
/* R6b ptrbig (2026-09-09): 指针返回 错误函数 8B 双通道 大错码 —
   原 指针错误函数 未登记 中细 8B (只 满平 ll) → 错误走 int 打包通道; 补登记后 8B 通道 (rax=码 8B + rdx=tag)
   错码 ≥0x80000000 (无符号 32 位模式) 零扩透传 — 与 ll 8B 对齐 (直返 + 尝试)
   @EXPECT out:FEAT PTRBIG8 OK */
构 X { 整 v; };
静 构 X gx;
静 无 整 码;
错误 构 X *mk(空) { 若 (码) 错 码; gx.v = 9; 返 &gx; }
错误 构 X *mid(空) { 尝试 mk(); 返 &gx; }
函数 整 主(空) {
    码 = 0x80000000u;
    构 X *w1 = (构 X *)(无 长)0x80000000ull;
    若 (mk() != w1) { 格式化 "FAIL 直返 大码\n"; 返 1; }
    若 (mid() != w1) { 格式化 "FAIL 尝试透传 大码\n"; 返 2; }
    码 = 0x44;
    构 X *w2 = (构 X *)(无 长)0x44ull;
    若 (mk() != w2) { 格式化 "FAIL 小码\n"; 返 3; }
    码 = 0xFFFFFFFFu;
    构 X *w3 = (构 X *)(无 长)0xFFFFFFFFull;
    若 (mid() != w3) { 格式化 "FAIL 全1码\n"; 返 4; }
    码 = 0;
    若 (mk() != &gx) { 格式化 "FAIL 成功指针\n"; 返 5; }
    格式化 "FEAT PTRBIG8 OK\n";
    返 0;
}
