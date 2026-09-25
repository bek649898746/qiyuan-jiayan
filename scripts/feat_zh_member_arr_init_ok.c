/* t12 (2026-09-25 启元 · 第 363 站 round 32):
   **静态结构体「单元素成员数组」与「标量带花括号」的 嵌套聚合初始化 未落地**

   病灶: `值布()` 判「数组字段」用 `新退 > 0 && 主复 > 新退`(总字节 > 元素尺寸),
   而单元素数组 `T a[1]` 的 现捌(8) == 域行数(8) 与标量 完全同形 ⟹ 判为非数组
   ⟹ 落「opaque 嵌套花括 跳配平」把 `{ 4.5 }` 整块跳过 ⟹ 值丢弃 (实测 0.000)。
   插桩铁证: [T12a] 主复=8 新退=8 词型=19(左花) ⟹ 数组判据为假、opaque 判据为真。

   修法: ① 补「字段是否数组」权威载体 `数组标`(照 A62 `字段浮待` 单点收口模式:
      `字段数组待` 由解析路径置位、`字段尺寸递归` + `联合字段` 双处消费清零)
      ② `值布` 主循环 + 零初始化循环 + `末凶一` 一律改用【旧 ∨ 新】合并式判据
      ③ 新增「标量字段 初始化器 带花括号」分支(剥一层再解析)

   灵敏度 (锚三关): 旧编译器 `BBDDA240` 上 本锚 输出 大量 0.000 / 0 ⟹ 必红;
   新编译器 `964C70EA` 逐字对 gcc。

   ⚠️ 输出合并单行 —— `@EXPECTED out:` 只保留最后一条 (第 253 站教训)。 */
// @EXPECTED exit:0
// @EXPECTED out:T12 4.500 42 4.500 90 4.500 7,8 4.500 4.500 6.500 3.500 2.500
#include <stdio.h>

typedef struct S1 { double a[1]; } S1;
static S1 s_a = { { 4.5 } };            /* 单元素 double 数组成员 */

typedef struct S2 { int a[1]; } S2;
static S2 s_d = { { 42 } };             /* 单元素 int 数组成员 */

typedef struct S3 { float a[1]; } S3;
static S3 s_e = { { 4.5f } };           /* 单元素 float 数组成员 */

typedef struct S4 { char a[1]; } S4;
static S4 s_f = { { 'Z' } };            /* 单元素 char 数组成员 */

typedef struct S5 { double a; } S5;
static S5 s_g = { { 4.5 } };            /* 标量字段 带花括号 (合法 C) */

typedef struct P { int x, y; } P;
typedef struct S6 { P a[1]; } S6;
static S6 s_h = { { { 7, 8 } } };       /* 单元素 结构体数组成员 */

typedef union U1 { double a[1]; } U1;
static U1 u_m = { { 4.5 } };            /* 联合成员 (走 联合字段 消费路径) */

typedef struct S7 { double a[1]; } S7;
static S7 arr_n[2] = { { { 4.5 } }, { { 6.5 } } };   /* 结构体数组元素 内含单元素数组 */

typedef struct S8 { double x[2]; double y[1]; } S8;
static S8 s_o = { { 1.5, 2.5 }, { 3.5 } };          /* 多元素 后跟 单元素 (顺序敏感) */

int main(void) {
    S7 s_j = { { 4.5 } };               /* 局部 auto */
    printf("T12 %.3f %d %.3f %d %.3f %d,%d %.3f %.3f %.3f %.3f %.3f\n",
           s_a.a[0], s_d.a[0], (double)s_e.a[0], (int)s_f.a[0], s_g.a,
           s_h.a[0].x, s_h.a[0].y,
           s_j.a[0], u_m.a[0], arr_n[1].a[0], s_o.y[0], s_o.x[1]);
    return 0;
}
