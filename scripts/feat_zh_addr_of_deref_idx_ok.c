/* feat_zh_addr_of_deref_idx_ok.c — 取址 `&(*X)[i]` 的 元素地址 (A2) 根治 2026-09-23 启元

   病灶（A2，在 963F9FDB 上必红）：
     形态 `&(*X)[1]`（**取址** + 显式解引用 + 下标）—— 算出的地址 = `X + i*4` 而**不是** `*X + i*4`
     ⟹ **少一次解引用**。变量基 / 成员基 / 箭头基 **全部**中招（5 格）。

     字节级铁证（「值读 vs 取址」双探针 objdump 差分，只差 `float v =` / `float *q = &`）：
       值读 ✅  `mov fpp,%rax` / **`mov (%rax),%rax`** / `add %r11,%rax` / `movss (%rax),%xmm0`
       取址 ❌  `mov fpp,%rax` / (**无读取**)          / `add %r11,%rax` / …
     ⟹ 取址版缺那条 `mov (%rax),%rax` ⟹ 基址错。

     根因：`例 节点_取址` 的 `素又一 == 12 || 14` 分支（`&数组[i]` / `&*p`）置
     `码生成免解引用 = 1` 后直接 `码生成(子0[数])`。而本形态的**下标节点内部基是显式解引用**
     ⟹ `码生成免解引用 == 1` 让它**只算自身地址而不加载**。
     修：`例 节点_下标` 的「解引用基」分支（进入条件即 `节型别[基] == 节点_解引用`）里
     把 `码生成(子0[数])` 包成 `码生成免解引用 = 0` 强制度值（镜像 A3 写侧的修法；该分支内基必为
     解引用节点 ⟹ 无条件强制是幂等的，值读路径逐字不变）。

   本锚覆盖：变量基 / 成员基（float*、double*）/ 箭头基（float*、double*）五种取址形态
   + 三种正确对照（`&fp[1]`、`&sBox.p[1]`、`&**fpp`）。
   灵敏度：在 963F9FDB 上 K/L/M 组共 5 格必红（得 0）。 */
// @EXPECTED out:A1=1 A2=1 A3=1 A4=1 A5=1 C1=1 C2=1 C3=1
// @EXPECTED exit:0
#include <stdio.h>

struct box { float *p; float **pp; double *dp; double **dpp; };

static float  fbuf[4] = { 1.5f, 2.5f, 3.5f, 4.5f };
static double dbuf[4] = { 1.25, 2.25, 3.25, 4.25 };
static float  *fp  = fbuf;
static float  **fpp = &fp;
static double *dp  = dbuf;
static double **dpp = &dp;

static struct box  sBox;
static struct box *sPtr;

int main(void) {
    sBox.p = fp;  sBox.pp = fpp;  sBox.dp = dp;  sBox.dpp = dpp;
    sPtr = &sBox;

    printf("A1=%d ", (int)(&(*fpp)[1]      == fbuf + 1));   /* 变量基 */
    printf("A2=%d ", (int)(&(*sBox.pp)[1]  == fbuf + 1));   /* 成员基 */
    printf("A3=%d ", (int)(&(*sPtr->pp)[1] == fbuf + 1));   /* 箭头基 */
    printf("A4=%d ", (int)(&(*sBox.dpp)[1] == dbuf + 1));   /* 成员基 double */
    printf("A5=%d ", (int)(&(*sPtr->dpp)[1]== dbuf + 1));   /* 箭头基 double */

    printf("C1=%d ", (int)(&fp[1]          == fbuf + 1));   /* 对照：无解引用 */
    printf("C2=%d ", (int)(&sBox.p[1]      == fbuf + 1));   /* 对照：成员指针字段直接下标 */
    printf("C3=%d\n", (int)(&**fpp         == fbuf));       /* 对照：双解引用无下标 */
    return 0;
}
