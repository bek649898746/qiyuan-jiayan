// @EXPECTED exit:0
// 回归: 本地类型定义结构体指针字段 brace 初始化 (fix 2026-08-07: 本地类型定义指针字段 frow=1)
int main() {
    struct Local { int v; struct Local *next; };
    struct Local a = {3, 0};
    struct Local b = {4, &a};
    if (b.v != 4) return 1;
    if (b.next->v != 3) return 2;
    return 0;
}
