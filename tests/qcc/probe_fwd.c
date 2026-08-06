// @EXPECTED exit:0
/* probe_fwd.c — 前置声明/先调用后定义 探测 */
int later(int x);

int main() {
    if (later(5) != 10) return 1;
    return 0;
}

int later(int x) {
    return x * 2;
}
