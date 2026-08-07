// @EXPECTED exit:0
// 回归: typedef 结构体变量 brace 初始化含指针字段 (fix 2026-08-07: 原 Nc(d,expr()) 不能解析 '{' → 字段从未写入)
typedef struct LNode { int v; struct LNode *next; } LN;
int main() {
    struct LNode a = {3, 0};
    LN b = {4, &a};
    if (b.v != 4) return 1;
    if (b.next->v != 3) return 2;
    return 0;
}
