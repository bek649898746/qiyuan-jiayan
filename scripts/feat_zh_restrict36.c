#包含 <stdio.h>

/* #36 显式别名管理 (restrict): 类型修饰符完整解析 + A5002 别名冲突检测
   restrict 指针声明 (int *restrict p = &x) — 编译期承诺: 基对象仅经此指针访问
   同一基对象被两个 restrict 指针占用 → A5002 (C 标准 restrict 语义) */

int 向量加(int *restrict 甲, int *restrict 乙, int n) {  /* 合法: 两个不同数组 */
    int 和 = 0;
    for (int i = 0; i < n; i++) 和 += 甲[i] + 乙[i];
    return 和;
}

int main(void) {
    /* ① 单 restrict 指针 + 正常使用 */
    int x = 42;
    int *restrict a = &x;
    *a = 7;
    if (x != 7) return 1;
    if (*a != 7) return 2;

    /* ② restrict 参数 (两个不同基) + 指针链 (int **restrict) */
    int arr1[3] = { 1, 2, 3 };
    int arr2[3] = { 10, 20, 30 };
    if (向量加(arr1, arr2, 3) != 66) return 3;

    int 基 = 5;
    int *restrict 内层 = &基;
    int **restrict 外层 = &内层;
    if (**外层 != 5) return 4;

    /* ③ 非 restrict 指针共享基 OK (无承诺) */
    int y = 9;
    int *p1 = &y;
    int *p2 = &y;
    *p1 = 10;
    *p2 = 11;
    if (y != 11) return 5;

    printf("#36 restrict 别名管理 PASS\n");
    return 0;
}
