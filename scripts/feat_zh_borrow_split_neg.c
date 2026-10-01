// @EXPECTED err:A3001
#include <stdio.h>

/* G4-1 补锚 (#33 借用分裂 · 负例面):
 *   可变借用 s.x 期间 读 s.x → 违反读写互斥 → A3001 (编译期 拦截)。
 *   (正例面 见 feat_zh_borrow_split_ok.c) */

struct 点 { int x; int y; };

int main(void) {
    struct 点 s;
    s.x = 5;
    s.y = 6;
    可变借用 s.x;
    if (s.x != 5) return 1;   /* ← 期望 编译失败 A3001 */
    return 0;
}
