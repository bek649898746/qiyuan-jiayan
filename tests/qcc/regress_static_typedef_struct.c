// @EXPECTED exit:0
// 回归: static typedef'd struct 局部变量 brace 初始化 (fix 2026-08-07: 原 static 前缀后 typedef 类型名被当变量名 + 注册成 int)
typedef struct LNode { int v; struct LNode *next; } LN;
int main() {
    static LN b = {4, 0};
    if (b.v != 4) return 1;
    if (b.next != 0) return 2;
    static LN c = {5, 0};
    if (c.v != 5) return 3;
    return 0;
}
