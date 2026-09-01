# 包含 <stdio.h>

/* #86 内存泄漏检测: 泄漏报告; → __qcc_leak_report() — 报告运行时资源泄漏
   (qcc_rt.c 管理的资源: arena 池未释放; qcc 内建 malloc 是提升分配器无泄漏概念) */

int main(void) {
    /* ① 正常释放 → 无泄漏 */
    int 区 = -1;
    区域 建(区, 1024);
    int *p = (int*)区域 分配(区, 4);
    p[0] = 7;
    if (p[0] != 7) return 1;
    区域 释放(区);
    泄漏报告;                    /* → __qcc_leak_report(): 无泄漏 */
    return 0;
}
