// @EXPECTED out:y.b=hi y.c=abc y.a=12
/* A71-续4 (2026-09-15): 局部结构体的「小数组字段」(总尺寸 ≤ 8) 点访问取值 —— **正向**。
   病灶: 局部 点访问 `变量.字段` 的读取路径里, 「数组字段 → 取地址」判据写成数值阈值 `域尺寸 > 8`,
         且上方「结构体值字段 域尺寸<=8」分支会 **先命中** ⟹ 总尺寸 ≤ 8 的数组字段
         (字符 b[8] / 字符 c[4] / 短整 s[4] / 整数 a[2] / 长整 l[2]) 被当 **8 字节标量读**:
         把数组内容当指针/值 → 运行期 0xC0000005 崩 或 静默错值。
         实测规律: 总尺寸 4/6/7/8 → 全错; 9/12/16 → 正确 (恰好是阈值分界)。
   根治: 把 数组判定 `末凶一(构序, 字段)` 提到该分支 **最前** (与箭头分支同一判据),
         数组字段在值上下文也退化到其地址 (C 语义), 并 剩未 布 2D 深度状态。
   本锚钉: 五种小数组字段 + 大数组字段 + 结构体整体赋值 后 读取, 与 gcc 逐字节一致。 */
#include <stdio.h>
#include <string.h>

struct S {
    char b[8];
    char c[4];
    short s[4];
    int a[2];
    long l[2];
    char big[16];
};

int main(void) {
    struct S x;
    struct S y;
    strcpy(x.b, "hi");
    strcpy(x.c, "abc");
    x.s[0] = 7; x.s[3] = 8;
    x.a[0] = 11; x.a[1] = 12;
    x.l[0] = 13; x.l[1] = 14;
    strcpy(x.big, "abcdefghij");
    printf("b=%s c=%s s=%d %d a=%d %d l=%ld %ld big=%s\n",
           x.b, x.c, x.s[0], x.s[3], x.a[0], x.a[1], x.l[0], x.l[1], x.big);
    y = x;
    printf("y.b=%s y.c=%s y.a=%d\n", y.b, y.c, y.a[1]);
    return 0;
}
/* @EXPECTED out:y.b=hi y.c=abc y.a=12 */
