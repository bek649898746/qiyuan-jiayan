#include <stdio.h>
// @EXPECTED exit:nonzero  /* 断言 失败 → 进程 终止 (exit 1) (地1 G0-1 补 意图标注 2026-09-11) */
// @EXPECTED out:BEFORE-ASSERT
/* 闸盲区修补 (2026-09-14): 断言 本身 **无消息输出**, 故 加 哨兵打印 ——
   证明 程序 跑到了 断言 之前 (排除 "在断言前就崩了 也 算 nonzero"); 而 "断言 无效" 会 走 到 printf("不应到达")
   并 return 0 ⟹ 由 exit:nonzero 兜住. 两 判据 合起来 才 排除了 硬崩 与 静默 放行 两 种 假通过。
   ⚠️ 断言行 必须 独立成行 —— 框架 正则 `out:(.+)$` 会 把 行尾 注释 一并 抓走。 */
int main(void) {
    int x = 5;
    printf("BEFORE-ASSERT\n");
    fflush(stdout);
    断言(x > 10);
    printf("不应到达\n");
    return 0;
}
