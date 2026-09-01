#包含 <stdio.h>

/* #36 restrict 负例: 同一基对象被两个 restrict 指针占用 → A5002 */

int main(void) {
    int x = 42;
    int *restrict a = &x;
    int *restrict b = &x;   /* A5002: 基对象 x 已被 a 占用 */
    if (*a != 42) return 1;
    if (*b != 42) return 2;
    return 0;
}
