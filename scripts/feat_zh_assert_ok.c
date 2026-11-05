// @EXPECTED out:PASS
#include <assert.h>
#include <stdio.h>

/* assert 家族 真接入 —— ORD-20261104-110
 *
 * 病灶：`<assert.h>` 在系统头白名单里 ⟹ `#include <assert.h>` 不报错，但 **assert 宏本身从未提供**
 *   ⟹ `assert(1);` 报 A1001 未声明函数（C 生态最基础的诊断宏之一，redis/hiredis、zlib examples 全中）。
 *   `_Static_assert` 原只支持 **顶层** 与 **struct 体内**；函数体（C11 6.7.2.1 允许）落 A1001。
 *
 * 修法：
 *   ① 系统头「跳过」处 **就地注入** assert 宏（NDEBUG 判定点 = include 点，与 C 语义一致）：
 *      `#ifndef NDEBUG / #define assert(x) ((x) ? 0 : (printf(...), exit(1), 0)) / #else ((void)0)`
 *   ② 语句() 分派口补块作用域 `_Static_assert`（与顶层同判据：常量求值 + 假则 A6017）。
 *
 * 本锚（正向面）：assert(真) 不触发、_Static_assert(真) 通过、表达式真的被求值一次。
 * 语义面（assert(假) 真炸 / NDEBUG 真静默 / _Static_assert(假) 真报错）由
 * `scripts/check_assert_semantics.py` 三路独立编译运行判。
 */

static int counter = 0;
static int bump(void) { counter = counter + 1; return 1; }

int main(void) {
    int bad = 0;

    assert(1);                    /* 真值：不触发、不打印 */
    assert(2 > 1);
    _Static_assert(1, "ok");      /* 块作用域 _Static_assert */

    bump();
    assert(bump() == 1);          /* 表达式必须真的求值一次 */
    if (counter != 2) { printf("FAIL eval %d\n", counter); bad++; }

    if (bad) { printf("FAIL total=%d\n", bad); return 1; }
    printf("PASS\n");
    return 0;
}
