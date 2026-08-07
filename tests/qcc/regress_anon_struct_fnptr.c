// @EXPECTED exit:0
// 回归: 匿名全局结构体 fnptr 字段 (fix 2026-08-07: C源+镜像原缺 fnptr 字段解析分支 → '(' 不被消费 → 死循环)
int add(int a, int b) { return a + b; }
struct { int (*cb)(int, int); } g = { add };
int main() {
    if (g.cb(2, 3) != 5) return 1;
    return 0;
}
