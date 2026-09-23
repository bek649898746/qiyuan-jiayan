/* selftest_main.c — jylib 自测驱动
 *
 * 用法：
 *   gcc -I include  tests/selftest_main.c  lib/jylib_sha256.gcc.obj  -o selftest
 *   gcc -I include  tests/selftest_main.c  lib/jylib_sha256.jy.obj   -o selftest
 *
 * 两个版本应当输出逐字一致 —— 这是「甲言编译版可信」的判据。
 */
#include <stdio.h>
#include "jylib_sha256.h"

int main(void) {
    int failures = jy_sha256_selftest();
    printf("; exit=%d\n", failures);
    return failures;
}
