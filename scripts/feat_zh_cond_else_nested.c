// @EXPECTED out:PASS
#include <stdio.h>

/* C69 补锚 — 条件编译「`#else` 跳过态 + 内部嵌套」崩溃面 (第 55 站, 2026-09-17)
 *
 * 根因: `#else`/`#elif` 落入「跳过」分支时只置 `顺一`(跳过标志), **未把
 *       `下劣[条件计数-1]`(本层当前是否编译) 置 0**; 而 `#endif` 的恢复式是
 *       `顺一 = !下劣[层-1]` ⟹ 弹栈时按「该层条件为真 = 编译」错误恢复
 *       ⟹ `#else` 体内**先出现的嵌套 `#endif`** 会把后续代码错误地放回编译流。
 * 真实载体: compat/nedmalloc/malloc.c.h —— `#ifndef WIN32` 真分支走完后, 其 `#else`
 *       体 (L1341-1413, 内含嵌套 `#if/#else/#endif`) 本应整体跳过, 但 L1404-1409
 *       的 `_InterlockedExchange` 被编译 ⟹ `_ReadWriteBarrier()` 报 A1001。
 * 本锚覆盖: ① 真分支 + else 内嵌套 (原泄漏面) ② 假分支走 else ③ elif 链判重
 *           ④ elifdef 后跟 else ⑤ 三层嵌套。
 */

/* ① 真分支已走 ⟹ `#else` 体整体跳过; 但 else 体内先有嵌套 `#if 0 ... #endif` */
static 整 真分支含嵌套(void) {
    整 r = 0;
#if 1
    r = 5;
#else
#if 0
    r = 6;
#endif
    r = 77;
#endif
    返 r;
}

/* ② 假分支 ⟹ 走 `#else` 体 (else 是默认路径), 体内同样有嵌套 */
static 整 假分支走否则(void) {
    整 r = 0;
#if 0
    r = 1;
#else
#if 0
    r = 2;
#endif
    r = 8;
#endif
    返 r;
}

/* ③ `#elif` 链判重: 第一个 elif 命中后, 后续 elif/else 必须全跳过 */
static 整 否则若链(void) {
    整 r = 0;
#if 0
    r = 91;
#elif 1
    r = 1;
#elif 1
    r = 33;
#else
    r = 44;
#endif
    返 r;
}

/* ④ `#elifdef` 后跟 `#else` (判重) */
#define C69_FLAG 1
static 整 否则若定义链(void) {
    整 r = 0;
#if 0
    r = 91;
#elifdef C69_FLAG
    r = 7;
#else
    r = 8;
#endif
    返 r;
}

/* ⑤ 三层嵌套 + else */
static 整 三层嵌套(void) {
    整 r = 0;
#if 1
#if 0
    r = 1;
#else
#if 1
    r = 3;
#else
    r = 4;
#endif
#endif
#else
    r = 99;
#endif
    返 r;
}

int main(void) {
    if (真分支含嵌套() != 5) return 1;
    if (假分支走否则() != 8) return 2;
    if (否则若链() != 1) return 3;
    if (否则若定义链() != 7) return 4;
    if (三层嵌套() != 3) return 5;

    printf("PASS\n");
    return 0;
}
