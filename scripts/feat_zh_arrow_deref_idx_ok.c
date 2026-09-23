/* feat_zh_arrow_deref_idx_ok.c — 箭头基 + 显式解引用 + 下标（值读 / 写） A3 根治 2026-09-23 启元

   病灶（A3，在 646FC353 上必红）：
     ① 值读 `(*pb->pp)[1]`：读路径守卫 `变量指元素(链根) > 8` 本意只排除「指针到数组」
        (`int (*pm)[2][3]`)，却**误伤**「指向 >8 字节结构体的指针」(`struct box *pb`，指元素=16/指内=0)
        ⟹ 被挡出「解引用基读」分支 ⟹ 落兜底路径 ⟹ 步长 8 + 8 字节读
        ⟹ 得 1024.00（应 2.50）。分界实测：结构体 8B ✓ / 16B ✗ / 24B ✗。
     ② 写 `(*pb->pp)[1] = v`：「解引用基写」分支里
        (a) 无 `表达式内重` 改道（读路径 R248/R249 有）⟹ 宽伍 = 结构体尺寸 16（应 float 的 4）
        (b) `码生成免解引用 == 1`（求左值地址上下文）里解引用节点只算自身地址而不加载
        ⟹ 目标地址少一次解引用 ⟹ 8 字节 double 位模式写到 `&fp`，**覆盖指针变量本身**
        （实测 fp: 0x433fb0 → 0x4021000000000000 = 8.5 的 double；后续读取即崩）。

   本锚覆盖：结构体三尺寸 × 箭头基 + 成员基对照 + 写侧 + 写后指针完整性。
   灵敏度：646FC353 上 R1/R3 得 1024.00、W1 得 2.50（写未生效）⟹ 必红。

   ⚠️ 本锚**刻意不含**「指针到数组 `int (*pm)[2][3]`」与结构体指针**同处一单元**的组合 ——
   实测该组合存在独立既存缺陷（`(*pm)[1][2]` 得垃圾，探针 `diffG.c`，旧/新编译器同错），
   属全局状态污染，另立挂账。指针到数组本身由既有闸锚 `feat_c_ptr_to_array_ok.c` 覆盖。 */
// @EXPECTED out:R1=2.50 R2=2.50 R3=2.50 R4=2.50 W1=8.50 W2=8.50 P1=ok
// @EXPECTED exit:0
#include <stdio.h>

struct box   { float *p; float **pp; };
struct box8  { float **pp; };
struct box24 { float **pp; int pad[4]; };

static float fbuf[4] = { 1.5f, 2.5f, 3.5f, 4.5f };
static float *fp = fbuf;
static struct box   bx;   static struct box   *pb  = &bx;
static struct box8  b8;   static struct box8  *p8  = &b8;
static struct box24 b24;  static struct box24 *p24 = &b24;

static float *fp0;

int main(void) {
    bx.pp = &fp;  b8.pp = &fp;  b24.pp = &fp;
    fp0 = fp;

    printf("R1=%.2f ", (*pb->pp)[1]);    /* 16B 结构体指针（病灶 ①） */
    printf("R2=%.2f ", (*p8->pp)[1]);    /*  8B（原就正确 · 回归哨兵） */
    printf("R3=%.2f ", (*p24->pp)[1]);   /* 24B（病灶 ①） */
    printf("R4=%.2f ", (*bx.pp)[1]);     /* 成员基（对照，原就正确） */

    fbuf[1] = 2.5f;
    (*pb->pp)[1] = 8.5f;                 /* 病灶 ②：写 */
    printf("W1=%.2f ", fbuf[1]);         /* 必须落到 fbuf[1]（旧编译器：写偏 + 污染 fp） */
    printf("W2=%.2f ", (*pb->pp)[1]);
    printf("P1=%s\n", (fp == fp0) ? "ok" : "CLOBBERED");   /* 写不得污染指针变量 */
    return 0;
}
