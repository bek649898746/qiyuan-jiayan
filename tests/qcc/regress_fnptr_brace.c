// @EXPECTED exit:0
// 回归: fnptr 字段 brace 初始化 (fix 2026-08-07: 单 fnptr frow=1 → brace_fields 数组路径崩)
int add(int a, int b) { return a + b; }
struct S { int (*cb)(int, int); };
int main() {
    struct S s = { add };
    if (s.cb(2, 3) != 5) return 1;
    return 0;
}
