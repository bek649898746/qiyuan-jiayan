// @EXPECTED out:PASS stmt-expr
#include <stdio.h>

/* A34 (2026-09-11, 差分 台 alignas 族 抓) 回归 锚: GNU 语句表达式 ({ ... })
 *   原 行为: 解析 **通过** (块内 变量 已 登记 — QCC_DUMP=ast 实证), 但 码生成 阶段 末表达式
 *     在 **块 作用域 之外** 求值 → 查变量 落空 → 当 函数名 → `未定义函数 't' 不能取地址` 退出。
 *   修法: 外层 声明 临时槽, 块内 把 末表达式 值 写 入 临时, 块外 读 临时 (语义 等价, 作用域 正确)。
 *   本 锚 覆盖: ① 单句 声明+算术 ② 两句 + 三元 ③ 后缀 乘法 ④ 块内 if ⑤ 浮点 值 ⑥ 嵌套 语句表达式 */

int main(void) {
    int a = 7;
    /* ① 单句 声明 + 算术 */
    int r1 = ({ int t = a * 2; t + 1; });
    if (r1 != 15) return 1;
    /* ② 两句 + 三元 */
    int r2 = ({ int u = 7; int v = 9; u > v ? u : v; });
    if (r2 != 9) return 2;
    /* ③ 后缀 运算符 作用于 语句表达式 */
    int r3 = ({ int t = a; t; }) * 3;
    if (r3 != 21) return 3;
    /* ④ 块内 控制流 */
    int r4 = ({ int t = a; if (t > 3) t = 1; t; });
    if (r4 != 1) return 4;
    /* ⑤ 浮点 值 */
    double r5 = ({ double d = 2.5; d * 2.0; });
    if (!(r5 == 5.0)) return 5;
    /* ⑥ 实参 位置 + 结果 参与 运算 */
    int r6 = ({ int n = 3; n; }) + ({ int m = 4; m; });
    if (r6 != 7) return 6;
    printf("PASS stmt-expr\n");
    return 0;
}
