// @EXPECTED exit:0
// @EXPECTED out:99
// fix 2026-08-12: int ** 双重指针局部变量声明 (原解析只消费一个 * → pp 未注册 → 残留垃圾)
int printf(const char*, ...);
int main() {
    int n = 5;
    int *p = &n;
    int **pp = &p;
    **pp = 99;
    printf("%d\n", n);
    // 双重指针数组缩放: pp[0] == &n (int* 元素, 4 字节)
    printf("%d\n", (*pp == p) && (**pp == 99));
    return 0;
}
