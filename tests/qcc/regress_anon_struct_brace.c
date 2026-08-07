// @EXPECTED exit:0
// 回归: 匿名全局结构体 brace 初始化 (fix 2026-08-07: 原初始化被整体跳过)
struct LNode { int v; struct LNode *next; };
struct { int v; struct LNode *next; } b = {4, 0};
int main() {
    struct LNode a = {3, 0};
    b.next = &a;
    if (b.v != 4) return 1;
    if (b.next->v != 3) return 2;
    return 0;
}
