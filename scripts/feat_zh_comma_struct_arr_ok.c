// @EXPECTED out:ok 步长 p2
/* feat_zh_comma_struct_arr_ok.c — 局部「逗号续声明符 + 结构体数组」登记三件套
 *
 * R354 第 355 站（2026-09-25）· 挂账 5b / 5c 根治 · 新编译器 PASS / 旧编译器必红
 *
 * 病灶：局部声明的「逗号续声明符 数组」分支（`末午(变节二, 短聚, 逆七);` 与 花括路径
 *       `末午(值列, 短聚二, 逆七二);`）对 1D 数组 三件登记**全缺**：
 *         ① 元素尺寸 —— 判据只认 字符/短/浮/长，结构体别名落兜底 4（应 16）⟹ 槽只开 8 字节
 *         ② 型序标  —— 完全不设（`末午` 置 -1）⟹ 赋值路径 `副癸 = -1` ⟹ 站内「结构体元素整块拷贝」兜底判据落空
 *         ③ 域行数[0] —— 只在多维（`> 1`）时登记 ⟹ 读侧 `计进 = 域行数[i] > 0 ? … : 长卯` 退回错误步长
 *       三缺合一：写入其实正确（16 字节整块拷贝），但 `larr[0].a` 读出**地址量级**（42b0d3）。
 *
 * 判据（全为可验证的数值/地址关系，不依赖任何格式）：
 *   · 步长 = 16（四种声明形态：逗号续声明符 / 逗号续声明符带前置标量 / 花括路径后续 / 全局对照）
 *   · 元素成员读 == 写入值（含第二个元素，证明槽不相邻污染）
 */
#include <stdio.h>

typedef struct { long long a, b; } S16;

S16 garr[2];
int fails = 0;

#define CHK(name, got, want) do { \
    long long gv = (long long)(got), wv = (long long)(want); \
    if (gv != wv) { printf("FAIL %s: got %lld want %lld\n", name, gv, wv); fails++; } \
    else printf("ok %s\n", name); \
} while (0)

int main(void) {
    S16 a, larr[2];          /* 逗号续声明符（标量在前）*/
    S16 p1, p2[2];           /* 逗号续声明符（另一组）*/
    S16 q1[2] = {{1, 2}, {3, 4}}, q2[2];  /* 首个声明符 带花括初始化 ⟹ q2 走 花括路径 */
    S16 solo[2];             /* 单声明符（对照，历来正确）*/

    a.a = 0x1111111111111111LL; a.b = 0x2222222222222222LL;
    larr[0] = a; larr[1] = a;
    p2[0] = a;
    q2[0] = a;
    garr[0] = a;
    solo[0] = a;

    CHK("步长 larr", (long long)((char *)&larr[1] - (char *)&larr[0]), 16);
    CHK("步长 p2",   (long long)((char *)&p2[1]   - (char *)&p2[0]),   16);
    CHK("步长 q2",   (long long)((char *)&q2[1]   - (char *)&q2[0]),   16);
    CHK("步长 solo", (long long)((char *)&solo[1] - (char *)&solo[0]), 16);
    CHK("步长 garr", (long long)((char *)&garr[1] - (char *)&garr[0]), 16);

    CHK("larr0.a", larr[0].a, 0x1111111111111111LL);
    CHK("larr0.b", larr[0].b, 0x2222222222222222LL);
    CHK("larr1.a", larr[1].a, 0x1111111111111111LL);
    CHK("p2[0].a", p2[0].a,   0x1111111111111111LL);
    CHK("q2[0].b", q2[0].b,   0x2222222222222222LL);
    CHK("garr0.a", garr[0].a, 0x1111111111111111LL);
    CHK("solo0.b", solo[0].b, 0x2222222222222222LL);

    if (fails) { printf("RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT ALLOK\n");
    return 0;
}
