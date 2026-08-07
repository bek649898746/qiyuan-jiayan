/* 甲言内核 — Tensor 持久池 (Gate 5)
 * 存/取/删 接口 + 固定哈希表 (O(1))
 * 池: DDR5 固定区域 (当前用 QEMU 128MB 中的 64MB)
 */

#ifndef KERNEL_TENSOR_H
#define KERNEL_TENSOR_H

/* ================================================================
 * 池配置
 * ================================================================ */
#define TPOOL_ENTRIES 64          /* 哈希表槽位数 */
#define TPOOL_BASE    0x02000000  /* 池基址 32MB (QEMU 128MB内) */
#define TPOOL_SIZE    (64 * 1024 * 1024)  /* 64MB */

/* ================================================================
 * 池条目 (编译时固定)
 * ================================================================ */
typedef struct {
    字  name[32];    /* tensor 名称 (key) */
    整  offset;      /* 池内偏移 */
    整  size;        /* 数据大小 */
    整  flags;       /* 0=empty 1=used */
} TPoolEntry;

/* ================================================================
 * 池操作接口
 * ================================================================ */

/* 哈希: djb2 */
整 tpool_hash(字 *key) {
    整 h = 5381;
    循环 (*key) { h = ((h << 5) + h) + *key; key++; }
    返 h & 63;  /* 64 槽 */
}

/* 存: key → 物理地址, 返回 offset (0=失败) */
整 tpool_store(TPoolEntry *tbl, 字 *key, 整 size) {
    整 idx = tpool_hash(key);
    整 probe = 0;
    循环 (probe < 64) {
        整 i = (idx + probe) & 63;
        若 (tbl[i].flags == 0) {
            /* 找到空槽 */
            整 j=0; 循环(key[j] && j<31){tbl[i].name[j]=key[j];j++;} tbl[i].name[j]=0;
            tbl[i].size = size;
            tbl[i].offset = TPOOL_BASE + i * (TPOOL_SIZE / 64);  /* 64 等分 */
            tbl[i].flags = 1;
            返 tbl[i].offset;
        }
        probe = probe + 1;
    }
    返 0; /* 表满 */
}

/* 取: key → offset (0=未找到) */
整 tpool_fetch(TPoolEntry *tbl, 字 *key) {
    整 idx = tpool_hash(key);
    整 probe = 0;
    循环 (probe < 64) {
        整 i = (idx + probe) & 63;
        若 (tbl[i].flags == 0) 返 0; /* 空槽=未找到 */
        若 (tbl[i].flags == 1) {
            整 match = 1; 整 j=0;
            循环(key[j] && j<31){若(tbl[i].name[j]!=key[j]){match=0;break;}j++;}
            若(match && tbl[i].name[j]==0) 返 tbl[i].offset;
        }
        probe = probe + 1;
    }
    返 0;
}

/* 删: key → 0(成功) / -1(未找到) */
整 tpool_delete(TPoolEntry *tbl, 字 *key) {
    整 idx = tpool_hash(key);
    整 probe = 0;
    循环 (probe < 64) {
        整 i = (idx + probe) & 63;
        若 (tbl[i].flags == 0) 返 -1;
        若 (tbl[i].flags == 1) {
            整 match = 1; 整 j=0;
            循环(key[j] && j<31){若(tbl[i].name[j]!=key[j]){match=0;break;}j++;}
            若(match && tbl[i].name[j]==0){ tbl[i].flags=0; 返 0; }
        }
        probe = probe + 1;
    }
    返 -1;
}

#endif /* KERNEL_TENSOR_H */
