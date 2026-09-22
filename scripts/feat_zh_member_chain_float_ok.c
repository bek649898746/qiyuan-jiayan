/* R253 看护锚：**成员 / 箭头 / 数组元素 链** 上的指针字段（float/double 单双分流）
   —— 覆盖 R253 六族缺陷（2026-09-22/23 · 第 253 站及其续站）：
     ① `**成员基`（点号 + 箭头）读标量 —— 原 **8 字节读**（实测 8.000002，应 1.5）
     ② `(*箭头基)[i]` / `(*数组元素基)[i]` —— 原 **步长 8**（实测 1024.000246）；
        **数组元素基**另有更重形态：**程序崩溃**（野地址，全无输出）
     ③ **匿名内层结构体**的 float 字段 —— 原走**整数读**（实测 0.000000，应 2.5）
        （根因：嵌套/内联路径的型词分派无 `float`，且指针元数据全不登记）
     ④ **三层内联体**（链3）的指针字段元数据 —— 原**全失**
     ⑤ 【族 E】`节点浮性` 的链式分支只认**链根变量**的双精度基类码 ⟹ 成员基双解引用**浮标未置**
        （`float v = **sBox.pp` 原得 1069547520 = 0x3FC00000 = 1.5f 位模式当整数）
     ⑥ 【族 F】写侧 `数组浮` + 存法/缩放宽同样只看链根 ⟹ `sArr[0].pp[0][1] = 8.5f` 值留 xmm0
        （写后读 = 0.000000；与 R252 的「8 字节 movsd 写 4 字节 float」完全同型）
   判据 = **逐字输出比对**（单条 `@EXPECTED out:`，读侧与写侧合并到同一行）。

   ⚠️ **刻意不用「浮点比较」**（如 `if (v != 1.5f)`）：R253 过程中实测该形态曾受族 E 影响
      （成员基双解引用落变量后比较走整数）；现族 E 已修，但本锚仍以 `printf` 逐字比对为准，
      以免把将来**别的**浮点比较缺陷误记到本锚上。 */
// @EXPECTED exit:0
// @EXPECTED out:MCF 1.500000 1.500000 1.250000 2.500000 2.500000 2.500000 2.500000 2.500000 2.500000 8.500000 9.500000
#include <stdio.h>

struct box { float **pp; double **dd; };
struct A2  { struct { float *x; } a; };
struct A3  { struct { struct { float *x; } b; } a; };

static struct box  sBox;
static struct box *sPtr;
static struct box  sArr[2];
static struct A2   sA2;
static struct A3   sA3;

static float  fbuf[4];
static double dbuf[4];
static float  *fp;  static float  **fpp;
static double *dp;  static double **dpp;

int main(void) {
    int i = 1;
    fbuf[0] = 1.5f;  fbuf[1] = 2.5f;  fbuf[2] = 3.5f;  fbuf[3] = 4.5f;
    dbuf[0] = 1.25;  dbuf[1] = 2.25;  dbuf[2] = 3.25;  dbuf[3] = 4.25;
    fp = fbuf;  fpp = &fp;  dp = dbuf;  dpp = &dp;

    sBox.pp = fpp;  sBox.dd = dpp;
    sPtr = &sBox;
    sArr[0].pp = fpp;  sArr[1].pp = fpp;
    sA2.a.x = fp;  sA3.a.b.x = fp;

    /* ①成员基双解引用(点/箭) ②解引用+下标(点/箭/数组元素) ③匿名内层字段 ④链3 ⑤双下标 */
    printf("MCF %f %f %f %f %f %f %f %f %f",
           (double)**sBox.pp,
           (double)**sPtr->pp,
           (double)**sBox.dd,
           (double)(*sBox.pp)[1],
           (double)(*sPtr->pp)[1],
           (double)(*sArr[i].pp)[1],
           (double)sA2.a.x[1],
           (double)sA3.a.b.x[1],
           (double)sArr[0].pp[0][1]);

    /* ⑥ 写侧：成员链上的浮点写（值准备 + 目标宽度存法 + 本层元素宽 三处收口）——
       读侧与写侧合并到同一行，用单条 out 断言一并看护 */
    { float w1;
      sArr[0].pp[0][1] = 8.5f;         /* 数组元素字段 基（外层基 = 下标节点）*/
      w1 = fbuf[1];
      sPtr->pp[0][1] = 9.5f;           /* 箭头基 */
      printf(" %f %f\n", (double)w1, (double)fbuf[1]); }

    printf("MEMBER CHAIN FLOAT OK\n");
    return 0;
}
