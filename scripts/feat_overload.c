#include <stdio.h>

/* _Generic 重载: 按第一个参数类型选函数 (C11 泛型选择 = 编译期重载) */
static int pick(int a) { return 1; }
static int pick_ll(long long a) { return 2; }
static int pick_dbl(double a) { return 3; }
static int pick_ptr(char *a) { return 4; }

#define PICK(x) _Generic((x), \
    int: pick, \
    long long: pick_ll, \
    double: pick_dbl, \
    char *: pick_ptr, \
    default: pick)(x)

int main(void) {
    int r1 = PICK(42);            /* int → pick → 1 */
    int r2 = PICK(42LL);          /* long long → 2 */
    int r3 = PICK(3.5);           /* double → 3 */
    char buf[8];
    int r4 = PICK(buf);           /* char * → 4 */
    printf("r1=%d r2=%d r3=%d r4=%d\n", r1, r2, r3, r4);
    if (r1 != 1) return 1;
    if (r2 != 2) return 2;
    if (r3 != 3) return 3;
    if (r4 != 4) return 4;
    printf("PASS\n");
    return 0;
}
