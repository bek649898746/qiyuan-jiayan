# 包含 <stdio.h>

/* #86 泄漏检测 — 泄漏面 锚 (原 名 feat_zh_leak86_neg.c, A24 命名 违规 修正 2026-09-11):
 *   本 锚 是 **运行期** 用例 (泄漏 存在 → 泄漏报告 应 打出 [LEAK]), 不是 编译失败 用例,
 *   按 闸 命名 纪律 (「`_neg` = 编译失败 期望」) 必须 改名。
 *   配套 正例 (正常释放 → 无泄漏) 见 feat_zh_leak86.c。
 *
 * ⚠️ 断言 面: `__qcc_leak_report()` 源码 写 stderr, 但 经 qcc 运行时 落 到 **stdout**
 *    (实测: 闸 捕获 的 p.stdout 里 有 [LEAK] 行) → 用 `@EXPECTED out:` 断言。
 *    (闸 于 2026-09-11 同时 新增 `@EXPECTED err:` 能力 — stdout/err 两 面 都 可 断言。) */

// @EXPECTED err:[LEAK] arena 池 0 未释放

int main(void) {
    int 区 = -1;
    区域 建(区, 1024);            /* 故意 不 释放 → 泄漏 */
    int *p = (int*)区域 分配(区, 4);
    p[0] = 42;
    if (p[0] != 42) return 1;

    /* 同一 进程 另 开 一 个 区 并 正常 释放: 泄漏 的 区 不 影响 其它 区 的 生命周期 */
    int 区二 = -1;
    区域 建(区二, 256);
    int *q = (int*)区域 分配(区二, 4);
    q[0] = 7;
    if (q[0] != 7) return 2;
    区域 释放(区二);

    /* 泄漏 的 区 仍 可 继续 分配 (报告 ≠ 自动回收) */
    int *r = (int*)区域 分配(区, 4);
    r[0] = 9;
    if (r[0] != 9) return 3;

    泄漏报告;                     /* → stderr [LEAK] arena 池 … 未释放 */
    printf("PASS\n");
    return 0;
}
