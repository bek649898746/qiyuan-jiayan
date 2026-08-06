// @EXPECTED exit:0
/* probe_fnptr.c — 函数指针 探测 */
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

int main() {
    int (*fp)(int, int);
    fp = add;
    if (fp(3, 4) != 7) return 1;
    fp = sub;
    if (fp(10, 4) != 6) return 2;
    return 0;
}
