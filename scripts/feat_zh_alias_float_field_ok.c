/* R256 看护锚 —— **typedef 别名 作 结构体/联合 字段类型** 的 浮性（字段浮标 + 值读 + 尺寸）
   ----------------------------------------------------------------------------------
   病灶（R256 刀1/3/4/5/6 根治）:

     ① **字段浮标缺失**（最响）：字段类型是 typedef 别名时
        （`typedef float F; struct S { F f; };` / `typedef union { F f; unsigned u; } FU;`）
        `型定表[名].是否浮标` **未被消费** ⟹ 字段 `副优 = 0` ⟹
          (a) 写 `s.f = 浮点值` 误报 A9002「双精度 值赋给整型字段」（**合法 C 被拦**，响亮）
          (b) 读/写 走整数通道 ⟹ **静默错值**（比 (a) 更危险）
        缺口是「同一语义 多套口径」的三处副本 —— 各分支当时都并列处理了
        「有符号字符 / 无符号 / 构标」，**唯独漏浮点**:
          · L18483 匿名 struct/union + typedef 形态
          · L17973 具名 struct/union 主路径
          · L4710  块级（函数内）局部类型定义
        对照：内置词（`float` / `double` / `单` / `双`）四条形态**本来就对** —— 缺口纯在「别名」。

     ② **嵌套成员基的值读**：`码生成浮` 的 `暂 == 15` 分支把 `节名表 + 子0[数]` 一律当
        **变量名**查（查变量/变量型序）。基是成员节点时（`lo.in.f` 的 子0 = `lo.in`）取到的是
        **字段名**（`in`）⟹ 浮点读打到无关地址 ⟹ 读到 xmm0 残留（实测 4.500001）。
        修 = 基非变量时用 `内存地址()` 递归求「链末字段地址」，再按字段元数据发射 movsd / movss+cvt。
        （位模式路径与值读路径是两条独立路径，故本锚位模式项与值读项分别断言。）

     ③ **`sizeof(typedef 浮点别名 *)`** 得 4（应 8）—— 别名四子分支里，构标与标量都按 `*` 置 8，
        唯浮标分支只消费 `*` 不取值（A49 把结果从「恒 8」改为「取型定表尺寸」时打回 4）。

   覆盖矩阵（逐项断言）:
     内置 float/double × struct/union
     typedef 别名 float/double × { 匿名 typedef(union/struct) · 具名 struct · 具名 union · 块级局部 · 嵌套两实例 }
     + 位模式核对（绕过 %f 与浮点通道）+ sizeof(别名浮点 *)

   全部合并进**一行**输出（闸的 `@EXPECTED out:` 只保留最后一条）。
   旧编译器（69356957）: 别名族报 A9002 编译失败、嵌套值为 4.500001、sizeof(F*)=4 ⟹ 必然 FAIL。 */
// @EXPECTED exit:0
// @EXPECTED out:AFT 3.500000 4.500000 40600000 40900000 40600000 40900000 3.500000 4.500000 7 9 8 8 3.500000 4.500000 3
#include <stdio.h>

typedef float  F;
typedef double D;

struct Sd { D d; unsigned u; };           /* 具名 struct + 别名 double */
union  Uf { F f; unsigned u; };           /* 具名 union  + 别名 float  */
typedef union { F f; unsigned u; } TFU;   /* 匿名 union  + typedef     */
typedef struct { D d; unsigned u; } TFS;  /* 匿名 struct + typedef     */

struct In  { F f; unsigned u; };
struct Out { struct In in; int tag; };

static struct Sd gsd;
static union  Uf guf;
static TFU gtfu;
static TFS gtfs;

int main(void) {
    struct Sd lsd;  union Uf luf;  TFU ltfu;  TFS ltfs;
    struct Out lo;  struct Out lb;
    F vf = 3.5f;
    D vd = 3.5;
    int sz1, sz2, sz3;

    /* ① 别名字段 写（修前：误报 A9002 ⟹ 编译失败） */
    lsd.d = vd;    gsd.d = 4.5;
    luf.f = vf;    guf.f = 4.5f;
    ltfu.f = vf;   gtfu.f = 4.5f;
    ltfs.d = vd;   gtfs.d = 4.5;

    /* 块级局部类型定义 + 别名 */
    {
        struct LS { D d; unsigned u; };
        struct LS ls;
        ls.d = vd;  ls.u = 0u;
        sz3 = (int)ls.d;
    }

    /* ② 嵌套两实例（值读通道） */
    lo.in.f = vf;   lo.in.u = 0u;   lo.tag = 7;
    lb.in.f = 4.5f; lb.in.u = 0u;   lb.tag = 9;

    /* ③ sizeof(别名浮点 *) */
    sz1 = (int)sizeof(F *);
    sz2 = (int)sizeof(D *);

    printf("AFT %f %f %x %x %x %x %f %f %d %d %d %d %f %f %d\n",
           lsd.d, gsd.d,
           luf.u, guf.u, ltfu.u, gtfu.u,
           (double)lo.in.f, (double)lb.in.f,
           lo.tag, lb.tag, sz1, sz2,
           ltfs.d, gtfs.d, sz3);
    return 0;
}
