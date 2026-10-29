/* ==========================================================================
 * jylib_hash.c — 非加密哈希核心实现
 *
 * 语料来源 : 旧壳武库 fnv.c + murmur3.c，合并改造
 * 改造内容 :
 *   ① 合并     —— 三个哈希函数合为一库
 *   ② 前缀     —— 统一 jy_
 *   ③ 去未对齐 —— 原 murmur3 用 *(uint32_t*)(d + i*4) 直接解引用，
 *                对未对齐输入是未定义行为；本版逐字节拼装
 *   ④ 宽容     —— len 由 int 改 unsigned long
 *   ⑤ 剥离     —— 去掉 main()
 * 位运算常数与轮数逐字沿用原实现（标准值，不能改）。
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件不含字符串字面量 —— 保持与外部工具链的最大兼容性
 * ⚠️ 零除法 —— 整数除法在部分工具链上会引入运行时库依赖，改用位运算
 * ⚠️ 只用明确宽度的整数类型
 * ---------------------------------------------------------------------------
 *
 * 编译方 : 甲言 qcc_x86      调用方 : 任意 C ABI 兼容工具链
 * ========================================================================== */
#include "jylib_hash.h"

/* ---- FNV-1a 标准常数 ------------------------------------------------ */
#define JY_FNV32_OFFSET  2166136261u
#define JY_FNV32_PRIME   16777619u
#define JY_FNV64_OFFSET  14695981039346656037ULL
#define JY_FNV64_PRIME   1099511628211ULL

/* ---- MurmurHash3 标准常数 ------------------------------------------- */
#define JY_MM3_C1 0xCC9E2D51u
#define JY_MM3_C2 0x1B873593u

/* ---- FNV-1a 32 ------------------------------------------------------- */
unsigned int jy_fnv32(const void *data, unsigned long len)
{
    unsigned int h = JY_FNV32_OFFSET;
    const unsigned char *p = (const unsigned char *)data;
    unsigned long i;

    if (p == 0) return h;
    for (i = 0; i < len; i++) {
        h ^= (unsigned int)p[i];
        h *= JY_FNV32_PRIME;
    }
    return h;
}

/* ---- FNV-1a 64 ------------------------------------------------------- */
unsigned long long jy_fnv64(const void *data, unsigned long len)
{
    unsigned long long h = JY_FNV64_OFFSET;
    const unsigned char *p = (const unsigned char *)data;
    unsigned long i;

    if (p == 0) return h;
    for (i = 0; i < len; i++) {
        h ^= (unsigned long long)p[i];
        h *= JY_FNV64_PRIME;
    }
    return h;
}

/* ---- 32 位循环左移 ---------------------------------------------------
 * r 必须 1..31；r==0 时 (x >> 32) 在 C 里是 UB，故单独处理。
 * -------------------------------------------------------------------------- */
static unsigned int jy_rotl32(unsigned int x, unsigned int r)
{
    if (r == 0u) return x;
    return (x << r) | (x >> ((32u - r) & 31u));
}

/* ---- 从字节数组拼一个 32 位小端字 -----------------------------------
 * 为什么不用 *(unsigned int*)p：未对齐访问是 UB。
 * 逐字节拼装虽然多几条指令，但任何平台、任何对齐都正确。
 * -------------------------------------------------------------------------- */
static unsigned int jy_load_le32(const unsigned char *p)
{
    return ((unsigned int)p[0])
         | ((unsigned int)p[1] << 8)
         | ((unsigned int)p[2] << 16)
         | ((unsigned int)p[3] << 24);
}

/* ---- MurmurHash3 x86_32 ---------------------------------------------
 * 算法与标准实现逐字一致（Austin Appleby 的参考实现）：
 *   每 4 字节成一块，做 k*=c1, rotl15, k*=c2, h^=k, rotl13, h=h*5+E6546B64
 *   尾部零头（1~3 字节）单独处理（等价于原实现的 switch 穿透）
 *   最后 h ^= len，三个 finalizer 步骤
 * -------------------------------------------------------------------------- */
unsigned int jy_murmur3_32(const void *key, unsigned long len, unsigned int seed)
{
    const unsigned char *d = (const unsigned char *)key;
    unsigned int h = seed;
    unsigned long nblocks;
    unsigned long i;
    unsigned long rem;
    unsigned int k;
    const unsigned char *tail;

    if (d == 0) return 0u;

    nblocks = len >> 2;                 /* len / 4，用移位避免除法 */
    for (i = 0; i < nblocks; i++) {
        k = jy_load_le32(d + (i << 2));
        k *= JY_MM3_C1;
        k = jy_rotl32(k, 15u);
        k *= JY_MM3_C2;

        h ^= k;
        h = jy_rotl32(h, 13u);
        h = h * 5u + 0xE6546B64u;
    }

    /* 尾部：等价于原 switch 的段段穿透（case3 → case2 → case1 累积） */
    rem = len & 3UL;
    tail = d + (nblocks << 2);
    k = 0u;
    if (rem == 3UL) {
        k ^= (unsigned int)tail[2] << 16;
        k ^= (unsigned int)tail[1] << 8;
        k ^= (unsigned int)tail[0];
    } else if (rem == 2UL) {
        k ^= (unsigned int)tail[1] << 8;
        k ^= (unsigned int)tail[0];
    } else if (rem == 1UL) {
        k ^= (unsigned int)tail[0];
    }
    if (rem != 0UL) {
        k *= JY_MM3_C1;
        k = jy_rotl32(k, 15u);
        k *= JY_MM3_C2;
        h ^= k;
    }

    /* finalizer */
    h ^= (unsigned int)(len & 0xFFFFFFFFUL);
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}
