// @EXPECTED out:n1=1 n2=2 n3=3 val=42
// @EXPECTED exit:0
#include <stdio.h>

/* 族1 #7 原子引用计数（Arc）—— 正例 (2026-10-02 第 444 站 · 原判 🔴 表层):
   本体 = 运行时 `qcc_rt.c` L805-824 真实现，计数用真原子 `原子 加`（lock xadd）:
     `共享原子` / `克隆原子` / `释放原子`。

   ★ 机制特有观测（区分性 · 第 444 站 skeptic 复核后补强）:
     直接读计数头（同 Rc 布局）: n1=1（仅共享原子）· n2=2（+一次克隆原子）· n3=3（+两次）。
     三点递增 ⟹ 原子计数随克隆真增长（独立通路，与 #6 分别验证）。
   ⚠️ 移动语义同 Rc（见 feat_zh_arc7_neg.c）。 */
int main(void) {
    整 v = 42;

    struct Rc ra = 共享原子(&v, 4);
    整 *pa = 取(ra);
    整 n1 = *(pa - 1);

    struct Rc rb = 共享原子(&v, 4);
    struct Rc rb2 = 克隆原子(rb);
    整 *pb = 取(rb2);
    整 n2 = *(pb - 1);

    struct Rc rc = 共享原子(&v, 4);
    struct Rc rc2 = 克隆原子(rc);
    struct Rc rc3 = 克隆原子(rc2);
    整 *pc = 取(rc3);
    整 n3 = *(pc - 1);

    printf("n1=%d n2=%d n3=%d val=%d\n", n1, n2, n3, *pc);
    return 0;
}
