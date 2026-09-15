// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* R6b (2026-09-09): 错误 struct X* 8B 指针载荷 — rax=指针/错码, rdx=tag
   尝试 传播 (指针 成功透传 + 错码透传); 比较即检查 消费;
   (挂账: 指针函数 8B 大错码(>32位) 9001 仍 int 打包截断 — 与 解构 tag 同族待 8B 深化;
    小错码/整 8B 大错码 均验证) */
struct X { int v; };
static struct X gx;
错误 struct X *mk(int c) { if (c < 0) 错 0x44; gx.v = 9; return &gx; }
错误 struct X *mid(int c) { 尝试 mk(c); return &gx; }
int main(void) {
    if (mid(1) != &gx) { printf("FAIL ok\n"); return 1; }      /* 成功: 指针透传 */
    if (mid(-1) != (struct X *)0x44) { printf("FAIL err\n"); return 2; }  /* 错码透传 */
    printf("PASS\n");
    return 0;
}
