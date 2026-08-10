// @EXPECTED exit: 6
// Phase 2-3: enum 常量数组维度 (int a[MAX])
enum { MAX = 3 };
int main(void) {
    int a[MAX] = {2, 4, 6};
    return a[0] + a[1] + a[2]; /* 12 */
}
