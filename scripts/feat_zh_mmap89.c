// @EXPECTED out:#89 mmap PASS
# 包含 <stdio.h>

/* #89 mmap 内存映射: 内存映射(路径, 大小) → __qcc_map_create (动态加载 kernel32
   CreateFileA 打开 + CreateFileMappingA 建映射 + MapViewOfFile 视图, 返回指针)
   取消映射(指针) → __qcc_map_close (UnmapViewOfFile) */

int main(void) {
    char path[512];
    const char *root = 取环境("QCC_TMP_ROOT");
    int *p;
    /* ① 创建映射文件 + 写 + 读回（A1：临时文件落 QCC_TMP_ROOT，不依赖 cwd） */
    if (!root || !root[0]) root = "D:\\qcc_tmp";
    sprintf(path, "%s/_mm89.bin", root);
    p = (int*)内存映射(path, 4096);
    if (!p) return 1;
    p[0] = 12345;
    p[1] = 67890;
    p[100] = 777;
    if (p[0] != 12345) return 2;
    if (p[1] != 67890) return 3;
    if (p[100] != 777) return 4;
    取消映射(p);

    /* ② 重新映射同一文件 → 数据持久 (内存映射文件落盘) */
    int *q = (int*)内存映射(path, 4096);
    if (!q) return 5;
    if (q[0] != 12345) return 6;   /* 文件内容持久 */
    if (q[100] != 777) return 7;
    取消映射(q);

    printf("#89 mmap PASS\n");
    return 0;
}
