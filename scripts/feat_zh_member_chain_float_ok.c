/* R253 看护锚：**成员 / 箭头 / 数组元素 链 上的指针字段**（float/double 单双分流）
   —— 覆盖 R253 四族缺陷（2026-09-22 · 第 253 站）：
     ① `**成员基`（点号 + 箭头）读标量 —— 原 **8 字节读**（实测 8.000002，应 1.5）
     ② `(*箭头基)[i]` / `(*数组元素基)[i]` —— 原 **步长 8**（实测 1024.000246）；
        **数组元素基**另有更重形态：**程序崩溃**（野地址，全无输出）
     ③ **匿名内层结构体**的 float 字段 —— 原走**整数读**（实测 0.000000，应 2.5）
        （根因：嵌套/内联路径的型词分派无 `float`，且指针元数据全不登记）
     ④ **三层内联体**（链3）的指针字段元数据 —— 原**全失**
   判据 = **逐字输出比对**（`@EXPECTED out:`）；任一格复发 ⟹ 输出行不符 ⟹ 锚变红。
   （刻意用 `printf` 而非 `if (v != 1.5f)` —— 见下。）

   ⚠️ **刻意不用「浮点比较」**：实测「成员基 双解引用」的结果**浮标未置** ⟹
      一旦落进变量或参与比较，就走**整数路径**：
        `float v = **sBox.pp;`  → v = 1069547520.000000（= 0x3FC00000 = 1.5f 的位模式当整数）
        `(double)**sBox.pp`     → 1.500000 ✓（printf 实参路径正确）
      对照：**变量基** `**fpp` 的同两种写法**都对**（N4/N8）⟹ 缺陷只在**成员基**。
      该族（**族 E**）**未修**，另记挂账；本锚只照已修的四族 ⟹ 锚应「红得其所」。

   ⚠️ 另一已知未修（不含在本锚内，另记挂账）：**写侧** `sArr[0].pp[0][1] = 8.5f`
      （成员链上的浮点写「值准备」判据仍按链根结构体 ⟹ 值留 xmm0 未落地）。 */
// @EXPECTED exit:0
// @EXPECTED out:MCF 1.500000 1.500000 1.250000 2.500000 2.500000 2.500000 2.500000 2.500000 2.500000
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
    printf("MCF %f %f %f %f %f %f %f %f %f\n",
           (double)**sBox.pp,
           (double)**sPtr->pp,
           (double)**sBox.dd,
           (double)(*sBox.pp)[1],
           (double)(*sPtr->pp)[1],
           (double)(*sArr[i].pp)[1],
           (double)sA2.a.x[1],
           (double)sA3.a.b.x[1],
           (double)sArr[0].pp[0][1]);
    printf("MEMBER CHAIN FLOAT OK\n");
    return 0;
}
