// @EXPECTED exit:0
/* probe_brk.c — continue/break 单层+嵌套探测 */
int main() {
    int i;
    int sum = 0;
    /* continue: 跳过 2 */
    for (i = 0; i < 6; i = i + 1) {
        if (i == 2) continue;
        sum = sum + i;
    }
    if (sum != 13) return 1;  /* 0+1+3+4+5 */
    /* break 嵌套循环 */
    for (i = 0; i < 10; i = i + 1) {
        if (i == 4) break;
    }
    if (i != 4) return 2;
    /* 嵌套: 内层 break 只跳内层 */
    {
        int j;
        int n = 0;
        for (i = 0; i < 3; i = i + 1) {
            for (j = 0; j < 10; j = j + 1) {
                if (j == 1) break;
                n = n + 1;
            }
        }
        if (n != 3) return 3;
    }
    return 0;
}
