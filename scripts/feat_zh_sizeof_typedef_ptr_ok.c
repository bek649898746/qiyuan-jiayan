// @EXPECTED out:p1=8,8 p2=8,8,8 p3=8,16 fp=8 arr=12 i32p=8
/* A48 锚 (2026-09-13, 不动点 504E5F0A → 待填): `尺寸(类型定义别名*)` 求值错乱 —— 静默错值。

   病灶: `尺寸()` 的 **typedef 名 分支** (4 个子分支: 构标 / 浮标 / 标量 / 未注册兜底) **都 不消费 `*`**,
     而 **类型词 分支** (L7410) **有** `若 (词型表[词位] == 乘) { 容薄 = 8; 词位++; ... }`。
     ⇒ `sizeof(别名*)` 的 `*` **残留 词元流** → 多实参 上下文 词元错位。
   症状 (修前实测, 全部 **静默错值**, gcc 对照见下):
     `printf("%d,%d\n", (int)sizeof(i64*), (int)sizeof(i64));`  甲言 `64,0`  / gcc `8,8`
     `printf("%d,%d,%d\n", sizeof(i64*), sizeof(i64*), sizeof(i64*));` 甲言 `64,0,0` / gcc `8,8,8`
     `printf("%d,%d\n", sizeof(i64*), sizeof(struct S));`       甲言 `64,0`  / gcc `8,16`
   **触发边界 (实测)**: `尺寸(别名*)` 在 **首位 + 后面还有实参** 时错; **单独** 或 **第二位** 时侥幸正确
     (残留的 `*` 被后续解析"恰好吃掉") ⇒ **偶发性错值**, 更难发现。
   **修法**: 4 个子分支 各自 消费 `*` 并令 结果 = 8 (指针语义), 标量分支 另 保存 类型名 (原 `词名表[词位-2]` 相对引用
     会因 `*` 消费而失效)。

   **覆盖**: ① 别名指针 首位+多实参 ② 别名指针 ×3 ③ 结构体指针 / 结构体本体 ④ 浮点别名指针
             ⑤ **typedef 数组本体** (数组数一>0 路径, 防改坏) ⑥ 标量别名指针 (i32*)。
   证据: 全部输出与 `gcc -w` 编同名程序逐字节全等。 */
#include <stdio.h>
typedef long long i64;
typedef int i32;
typedef double f64;
typedef int iarr3[3];

struct S { i64 a; i32 b; };

int main(void) {
    i64 x = 5;
    i64 *p = &x;
    f64 d = 1.5;
    f64 *pd = &d;
    struct S s; s.a = 1; s.b = 2;
    struct S *ps = &s;
    iarr3 arr; arr[0] = 1; arr[1] = 2; arr[2] = 3;
    printf("p1=%d,%d p2=%d,%d,%d p3=%d,%d fp=%d arr=%d i32p=%d\n",
        (int)sizeof(i64*), (int)sizeof(i64),
        (int)sizeof(i64*), (int)sizeof(i64*), (int)sizeof(i64*),
        (int)sizeof(struct S*), (int)sizeof(struct S),
        (int)sizeof(f64*), (int)sizeof(iarr3), (int)sizeof(i32*));
    (void)p; (void)pd; (void)ps; (void)arr;
    return 0;
}
