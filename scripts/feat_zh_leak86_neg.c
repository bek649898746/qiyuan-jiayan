# 包含 <stdio.h>

/* #86 泄漏检测 负例: arena 未释放 → 泄漏报告 检出 (stderr [LEAK]) */

int main(void) {
    int 区 = -1;
    区域 建(区, 1024);            /* 未释放 → 泄漏 */
    int *p = (int*)区域 分配(区, 4);
    p[0] = 42;
    if (p[0] != 42) return 1;
    泄漏报告;                     /* → [LEAK] arena 池 0 未释放 */
    return 0;
}
