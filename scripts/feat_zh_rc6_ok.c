// @EXPECTED out:a=42 b=7
// @EXPECTED exit:0
#include <stdio.h>

/* 族1 #6 引用计数（Rc）—— 正例（2026-10-02 第 444 站 · 原判 🔴 表层）:
   本体 = 运行时 `srclib\qcc_rt.c` L779-797 真实现（`malloc(sz+4)` + 前 4B 计数）：
     `struct Rc 共享(void *init, int sz)` 分配并拷贝初值;
     `struct Rc 克隆(struct Rc r)` 计数 +1（不复制数据）;
     `void 释放(struct Rc r)` 计数 -1，归零则 free;
     `void *取(struct Rc r)` 取载荷指针。
   判据值 (区分性): a=42 证 —— 共享持 **快照拷贝**（v 事后改为 7 不影响 r1）; 若 `共享` 退化为直通 &v 则 a=7。
                  b=7  证 —— 克隆链 r3 ← 克隆(r2)，r2 是 v=7 时共享 ⟹ 载荷 7。
   ⚠️ 移动语义: 甲言把 struct 实参按**所有权移动**（`克隆(r)` 消费 r）⟹ 同一引用不可重复使用（负例面见 _neg）。 */
int main(void) {
    int v = 42;
    struct Rc r1 = 共享(&v, 4);
    v = 7;                       /* 改原值 → 共享 应持快照 */
    struct Rc r2 = 共享(&v, 4);
    struct Rc r3 = 克隆(r2);     /* r2 消费, r3 有效 */
    printf("a=%d b=%d\n", *(int *)取(r1), *(int *)取(r3));
    return 0;
}
