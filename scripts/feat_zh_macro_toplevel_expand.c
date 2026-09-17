// @EXPECTED out:PASS
#include <stdio.h>

/* C70 补锚 — 宏展开「函数声明启发式」的跨行误判 (第 57 站, 2026-09-17)
 *
 * 根因: `副组` 的「函数声明启发式」在判断「宏名前面最近的非空白词是不是类型关键字」时,
 *       跳过空白用了 `' '||'\t'||'\r'||'\n'` —— **跨行**取到上一行的词。
 *       于是 `#define CTYPE(x) int` 之后一行的 `CTYPE(foo) ...` 会取到上一行的 `int`,
 *       被误判成「函数声明原型」⟹ **不展开**。
 *       (实参首字符是数字/字符串时, 另一条规则会把它救回 —— 所以只有标识符实参暴露。)
 * 本锚覆盖: ① 顶层 + 标识符实参 ② 顶层 + 无参函数宏 ③ 宏作函数返回类型
 *           ④ 函数内 (修复前就 OK, 防回归) ⑤ 实参是数字 (对照)
 */

#define CTYPE(x) int
#define CEMPTY() int

CTYPE(foo) 全局一 = 11;
CEMPTY() 全局二 = 22;
CTYPE(0) 全局三 = 33;

CTYPE(bar) 取一(void) { return 44; }

static int 取二(void) {
    CTYPE(baz) 局 = 55;
    返 局;
}

int main(void) {
    if (全局一 != 11) return 1;
    if (全局二 != 22) return 2;
    if (全局三 != 33) return 3;
    if (取一() != 44) return 4;
    if (取二() != 55) return 5;

    printf("PASS\n");
    return 0;
}
