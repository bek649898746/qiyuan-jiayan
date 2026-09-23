/* gate_gcc_link_module.c — 闸外检查用**模块**（甲言编译成 .obj，再由 gcc 链接）
 *
 * 为什么需要这个检查（第 262 站 ③ 的教训）:
 *   闸（run_tests_zh.py）只做「单文件 → 甲言编译 → 运行」——
 *   而本缺陷只在「甲言 -c → gcc/ld 链接（高基址 0x140000000 / 运行时 0x7ff6...）」时暴露:
 *   甲言自链自己基址低（0x400000 量级），32 位截断后恰好仍合法 ⟹ 闸永远测不出来。
 *
 * 本模块的构成**刻意命中三条历史路径**:
 *   · 一张 const 常量表   ⟹ 产生数据段 ⟹ 触发 __qcc_ginit 守卫（③ 的原始触发点）
 *   · 一次 memset 调用    ⟹ 内联 memset（③ 的另一个触发点）
 *   · 一次 memcpy 调用    ⟹ 内联 memcpy（③ 同族）
 *   · 一次 strcmp 调用    ⟹ 内联 strcmp（③ 同族）
 *
 * 期望（与 gcc 编译版一致）:
 *   G1=428A2F98  G2=00000000  G3=1  G4=OK
 */
#include <string.h>

static const unsigned int TAB[64] = {
    0x428A2F98,0x71374491,0xB5C0FBCF,0xE9B5DBA5, 0x3956C25B,0x59F111F1,0x923F82A4,0xAB1C5ED5,
    0xD807AA98,0x12835B01,0x243185BE,0x550C7DC3, 0x72BE5D74,0x80DEB1FE,0x9BDC06A7,0xC19BF174,
    0xE49B69C1,0xEFBE4786,0x0FC19DC6,0x240CA1CC, 0x2DE92C6F,0x4A7484AA,0x5CB0A9DC,0x76F988DA,
    0x983E5152,0xA831C66D,0xB00327C8,0xBF597FC7, 0xC6E00BF3,0xD5A79147,0x06CA6351,0x14292967,
    0x27B70A85,0x2E1B2138,0x4D2C6DFC,0x53380D13, 0x650A7354,0x766A0ABB,0x81C2C92E,0x92722C85,
    0xA2BFE8A1,0xA81A664B,0xC24B8B70,0xC76C51A3, 0xD192E819,0xD6990624,0xF40E3585,0x106AA070,
    0x19A4C116,0x1E376C08,0x2748774C,0x34B0BCB5, 0x391C0CB3,0x4ED8AA4A,0x5B9CCA4F,0x682E6FF3,
    0x748F82EE,0x78A5636F,0x84C87814,0x8CC70208, 0x90BEFFFA,0xA4506CEB,0xBEF9A3F7,0xC67178F2
};

static unsigned char 中[64];

/* G1: 常量表读（数据段 + 高地址取址） */
int gate_tab(int i) { return (int)TAB[i & 63]; }

/* G2: memset 内联（③ 直接触发点） */
int gate_zero(void) {
    unsigned char *p = 中;
    memset(p, 0, 64);
    return (int)中[63];
}

/* G3: memcpy 内联（同族） */
int gate_copy(void) {
    unsigned char 源[8]; unsigned char 的[8];
    源[0] = 7; 源[1] = 9; 源[2] = 0; 源[3] = 0;
    源[4] = 0; 源[5] = 0; 源[6] = 0; 源[7] = 0;
    memcpy(的, 源, 8);
    return (的[0] == 7 && 的[1] == 9) ? 1 : 0;
}

/* G4: strcmp 内联（同族）—— ⚠️ 刻意**不用字符串字面量**:
 *   本仓已知缺陷 ①: 字符串字面量生成 ADDR32（32 位绝对重定位）⟹ gcc/ld 拒绝链接。
 *   故用字符数组初始化（等价语义，不触发 ①）—— 这样本检查只测 ③ 那条路径。 */
static char 甲[6] = { 'G', '4', '-', 'O', 'K', 0 };
static char 乙[6] = { 'G', '4', '-', 'O', 'K', 0 };
int gate_cmp(void) { return strcmp(甲, 乙) == 0 ? 1 : 0; }
