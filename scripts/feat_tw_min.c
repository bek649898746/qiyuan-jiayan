// @EXPECTED out:sizes typedef: s8=1 ll=8 ff=4 ull=8
/* A49 (2026-09-13): 钉住 `typedef float 别名` 的 sizeof。
   病灶: `尺寸()` 的 **typedef 名 浮标分支** 硬编码 8 ⇒ `sizeof(ff)` (ff = typedef float) 得 8 (应 4);
     而 **非别名** `sizeof(float)` (类型词路径) 一直是对的 = **两形态不一致** (A15-余-b 家族: 别名/变量/字面量 三条路径, 改一漏二)。
   修: 按 `型定表[型定序].尺寸` 分流 (4 = float / 8 = double), 兜底 8。
   ⚠️ 本 锚 **只断言 sizes typedef 行** —— 同文件 的 `sizeof(1.0f)` 仍 返 8 (甲言 有意 把 float 字面量 当 double: 词法层 直接 丢弃 `f` 后缀);
      该 形态 已 挂账 **A50** (独立边缘形态: 需 给 浮点字面量 加 单精度标记, 复现见 scratch/_a49_find3.py)。
   证据: 该行 与 gcc 输出 逐字节一致。 */
#include <stdio.h>
typedef signed char s8;
typedef long long ll;
typedef float ff;
typedef unsigned long long ull;
int main(void) {
    printf("sizes: char=%d short=%d int=%d long=%d llong=%d float=%d double=%d ptr=%d\n",
        (int)sizeof(char), (int)sizeof(short), (int)sizeof(int), (int)sizeof(long),
        (int)sizeof(long long), (int)sizeof(float), (int)sizeof(double), (int)sizeof(int*));
    printf("sizes typedef: s8=%d ll=%d ff=%d ull=%d\n",
        (int)sizeof(s8), (int)sizeof(ll), (int)sizeof(ff), (int)sizeof(ull));
    printf("sizes expr: sizeof(1LL)=%d sizeof(1.0f)=%d sizeof(1.5)=%d\n",
        (int)sizeof(1LL), (int)sizeof(1.0f), (int)sizeof(1.5));
    long long a = 0x1234567890LL;
    unsigned long long b = 0xFFFFFFFFFFFFFFFFULL;
    printf("a=%lld b=%llu\n", a, b);
    long long c = 1000000000000LL;
    printf("c=%lld\n", c);
    float f = 3.5f;
    printf("f=%f\n", f);
    return 0;
}
