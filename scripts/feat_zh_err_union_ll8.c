// @EXPECTED exit:0
#include <stdio.h>
/* R6b (2026-09-09): 错误 长 长 8B 载荷 — 值占满 rax, tag 走 rdx 双通道 {rax 值 | rdx tag}
   3 层 尝试 传播: 8B 错码全值透传 (9001 传播 rax=错码 + rdx=1);
   成功值 == 错码 碰撞 → 尝试 判别 rdx tag (__e_t==1) 不误传播;
   (改前: 8B 返回未登记返宽 → R6b-1 返回侧死码 + 尝试 判 rax 高32 当 tag → 碰撞误传播) */
错误 long long leaf(long long x) { if (x < 0) 错 0x1122334455667788; return 0x1122334455667788; }
错误 long long mid(long long x) { 尝试 leaf(x); return 7; }
错误 long long top(long long x) { 尝试 mid(x); return 0; }
int main(void) {
    if (mid(5) != 7) { printf("FAIL mid-ok\n"); return 1; }           /* 成功值==错码碰撞, tag 区分 → 不误传播 */
    if (top(5) != 0) { printf("FAIL top-ok\n"); return 2; }
    if (mid(-1) != 0x1122334455667788) { printf("FAIL mid-err\n"); return 3; }       /* 8B 错码 透传 */
    if (top(-1) != 0x1122334455667788) { printf("FAIL top-err\n"); return 4; }       /* 3 层 传播 */
    printf("PASS\n");
    return 0;
}
