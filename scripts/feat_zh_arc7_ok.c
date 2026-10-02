// @EXPECTED out:a=42 b=7
// @EXPECTED exit:0
#include <stdio.h>

/* 族1 #7 原子引用计数（Arc）—— 正例（2026-10-02 第 444 站 · 原判 🔴 表层）:
   本体 = 运行时 `qcc_rt.c` L805-824 真实现，计数用真原子 `原子 加`（lock xadd）:
     `共享原子` / `克隆原子` / `释放原子`（同 Rc 布局，多线程并发安全）。
   判据值 (区分性): a=42（原子共享持快照拷贝）· b=7（原子克隆链载荷）—— 同一 Rc 形，独立通路。
   ⚠️ 移动语义同 Rc（见 feat_zh_arc7_neg.c）。 */
int main(void) {
    int v = 42;
    struct Rc r1 = 共享原子(&v, 4);
    v = 7;
    struct Rc r2 = 共享原子(&v, 4);
    struct Rc r3 = 克隆原子(r2);
    printf("a=%d b=%d\n", *(int *)取(r1), *(int *)取(r3));
    return 0;
}
