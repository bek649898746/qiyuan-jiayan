// @EXPECTED exit:0
// 回归: 结构体 fnptr 字段用变量实参调用 (fix 2026-08-07)
//   旧镜像 case-4 两段扫描: 先找 nt==1 的 child → 抓到尾参 y 当 callee
//   host C 版 08-03 已改单次"取最后 child", 镜像漏 → v1/v2 编此程序错选 callee
//   本测试用 变量实参 (非字面量) 触发: s.cb(x, y) 中 y 是 int 变量 (nt==1)
int add(int a, int b) { return a + b; }
struct S { int (*cb)(int, int); };
int main() {
    struct S s = { add };
    int x = 20, y = 22;
    if (s.cb(x, y) != 42) return 1;  /* 旧镜像会把 y 当 callee → 错编 */
    if (s.cb(x, 1) != 21) return 2;  /* 尾参为字面量 (nt==0) 不触发旧 bug */
    return 0;
}
