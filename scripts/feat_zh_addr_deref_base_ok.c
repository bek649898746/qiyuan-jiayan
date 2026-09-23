// @EXPECTED out:ADDR DEREF OK
// @EXPECTED exit:0
/* A2 刀1 (2026-09-23 第 261 站) —— **取址 解引用基 `&*p` / `&**p` 必须产出正确地址**.
   病灶: `例 节点_取址` 的兜底分支**不处理** `素又一 == 12`（解引用基）——
     `变名 = (字*)(节名表 + 子0[数])` 解引用节点**没有名字** ⟹ 取到空串 ⟹ `查变量("")` = -1
     ⟹ 兜底 两个 子分支 都 不命中 ⟹ **完全 不生成代码** ⟹ rax 保持残留值。
   实测（修前）: `&**fpp` = 43c10e（另一进程地址量级），gcc = fbuf。
   修: 与既有 `素又一 == 14`（`&数组[i]`）同构 —— 免解引用模式生成解引用节点。
   灵敏度: 旧编译器（8A7765FC）上 本锚 **必红**（地址为垃圾值）。
   ⚠️ 本刀 **不覆盖** `&(*X)[i]`（5 格）与 `(*箭头->pp)[i]` 值读 —— 属另一病灶, 已挂账。 */
#include <stdio.h>

struct box { float *p; float **pp; double *dp; double **dpp; };

static float  fbuf[4] = { 1.5f, 2.5f, 3.5f, 4.5f };
static double dbuf[4] = { 1.25, 2.25, 3.25, 4.25 };
static float  *fp  = fbuf;
static float  **fpp = &fp;
static struct box  bx;
static struct box *pb = &bx;

int main(void) {
    bx.p = fbuf; bx.pp = &fp; bx.dp = dbuf; bx.dpp = &bx.dp;
    if (&**fpp != fbuf)     { printf("f1 %p %p\n", (void*)&**fpp, (void*)fbuf); return 1; }
    if (&**bx.pp != fbuf)   { printf("f2 %p %p\n", (void*)&**bx.pp, (void*)fbuf); return 2; }
    if (&**pb->pp != fbuf)  { printf("f3 %p %p\n", (void*)&**pb->pp, (void*)fbuf); return 3; }
    if (&**bx.dpp != dbuf)  { printf("f4 %p %p\n", (void*)&**bx.dpp, (void*)dbuf); return 4; }
    if (&**pb->dpp != dbuf) { printf("f5 %p %p\n", (void*)&**pb->dpp, (void*)dbuf); return 5; }
    printf("ADDR DEREF OK\n");
    return 0;
}
