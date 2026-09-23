/* R255 看护锚 —— `(双精度)*指针` 的**解引用加载通道**
   -----------------------------------------------------------
   病灶（R255 刀2 根治，不动点 B9EB4BE0 → 8E3F3924）:
     `(双精度)*plp`（plp 指向 `长整长整`）原得 **指针值**（`*` 的加载完全没发生）。
     链条: 解析 `(双精度)X` 时把 `节浮标` 打在 **操作数（解引用节点）自身** 上
     ⟹ `节点浮性` ① 缓存 `若 (节浮标[节]) 返 1` 命中 ⟹ `例 节点_解引用` 改发 `movsd`
     （**只写 xmm0，rax 原封不动 = 指针值**）⟹ 而整数提升的判据只看 `表达式基浮`（类型推导=0）
     ⟹ 提升仍执行 ⟹ 随后 `cvtsi2sd xmm0, rax` 用指针值覆盖 xmm0。
     修 = `节点浮性` ① 缓存遵从**源类型**（解引用 + 链根指向非浮 ⟹ 不信任缓存）。

   本锚覆盖: 局部/全局 × 指向整数/指向双精度 × 解引用/下标 × 整数读/中间变量，
   全部合并进**一行**输出（闸的 `@EXPECTED out:` 只保留最后一条）。

   旧编译器（B9EB4BE0）: 第 2/3 项得指针值（每次不同）⟹ 必然 FAIL —— 红得其所。 */
// @EXPECTED exit:0
// @EXPECTED out:DCD 11 11.000000 11.000000 1.250000 1.250000 22.000000 22.000000 11.000000
#include <stdio.h>

static long long  lbuf[4];
static double     dbuf[4];
static long long *gplp;
static double    *gdp;

int main(void) {
    long long *plp;
    double    *pd;
    long long  v;
    lbuf[0] = 11; lbuf[1] = 22;
    dbuf[0] = 1.25; dbuf[1] = 2.25;
    plp = lbuf; gplp = lbuf; pd = dbuf; gdp = dbuf;
    v = *plp;
    printf("DCD %d %f %f %f %f %f %f %f\n",
           (int)*plp, (double)*plp, (double)*gplp, (double)*pd, (double)*gdp,
           (double)plp[1], (double)gplp[1], (double)v);
    return 0;
}
