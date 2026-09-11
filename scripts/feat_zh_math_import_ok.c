#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

/* A31 (2026-09-11, 差分 通道 抓) 回归 锚: 导入表 已 有 槽 的 函数 必须 可 隐式 声明 调用
 *   原 行为: 13 个 名字 (asin/acos/sinh/cosh/tanh/log10/hypot/copysign/iscntrl/isgraph/
 *     isprint/ispunct/strnlen) 在 导入表 里 有 槽, 但 隐式声明 白名单 后聚[] 漏 收 →
 *     调用 直接 A1001 未声明函数 (能力 缺口; 差分 测试台 math 族 因此 17 例 编译失败)。
 *   本 锚 覆盖 三 类: 三角/反三角 (tanh/sinh/cosh/asin/acos), 双参数 (hypot/copysign), 字符串/ctype。 */

int main(void) {
    /* 三角/反三角: 已知 值 对照 */
    if (!(tanh(0.0) == 0.0)) return 1;
    if (!(sinh(0.0) == 0.0)) return 2;
    if (!(cosh(0.0) == 1.0)) return 3;
    if (!(asin(0.0) == 0.0)) return 4;
    if (!(acos(1.0) == 0.0)) return 5;
    /* 双参数 */
    if (!(hypot(3.0, 4.0) == 5.0)) return 6;
    if (!(copysign(3.0, -1.0) == -3.0)) return 7;
    /* 对数 */
    if (!(log10(100.0) == 2.0)) return 8;
    /* 字符串 / ctype */
    if (strnlen("abc", 5) != 3) return 9;
    if (strnlen("abc", 2) != 2) return 10;
    if (iscntrl('\n') == 0) return 11;
    if (isgraph(' ') != 0) return 12;
    if (isprint(' ') == 0) return 13;
    if (ispunct(',') == 0) return 14;
    printf("PASS math-import\n");
    return 0;
}
