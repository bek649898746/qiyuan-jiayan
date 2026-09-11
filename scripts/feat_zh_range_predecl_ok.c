#include <stdio.h>

/* D1 (2026-09-11) 回归 锚: 遍-范围 迭代变量 **用户 预声明** (整 x;) 时 不 得 误报 A4001。
 * 原 缺陷: 4 处 注册点 写成 `若 (查变量(x) < 0) { 变量偏移(x); 未初始化移除(x); }`
 *          → 变量 已 由 用户 声明 时 整块 跳过 → 循环体 读 x → A4001 假阳
 *            (原 闸外 锚 feat_range3/4/6/7 长期 挂红 即 此 因, 闸 覆盖 制度化 时 抓 出)
 * 修法: 注册 仍 只在 未声明 时; 未初始化移除 无条件 (对 不存在变量 是 安全 空操作)。 */

typedef struct { int *data; int len; } Sl;

int main(void) {
    /* 1. 数组 值 迭代 (预声明 整 x;) */
    int a[5] = {1, 2, 3, 4, 5};
    int x;
    int s = 0;
    遍 x 于 a { s += x; }
    printf("arr=%d\n", s);
    if (s != 15) return 1;

    /* 2. 数组 索引+值 迭代 (两 变量 均 预声明) */
    int k, v;
    int t = 0;
    遍 k, v 于 a { t += k * v; }
    printf("idx=%d\n", t);
    if (t != 0 * 1 + 1 * 2 + 2 * 3 + 3 * 4 + 4 * 5) return 2;

    /* 3. 整数 区间 迭代 (预声明) */
    int i;
    int r = 0;
    遍 i 于 0 到 5 { r += i; }
    printf("range=%d\n", r);
    if (r != 10) return 3;

    /* 4. 切片 迭代 (预声明) */
    Sl sl;
    sl.data = a;
    sl.len = 5;
    int y;
    int q = 0;
    遍 y 于 sl { q += y; }
    printf("slice=%d\n", q);
    if (q != 15) return 4;

    /* 5. 循环体 为空 时 预声明 变量 仍 可 编译 (语义 从 既有 约定: 糖 视 为 已 初始化) */
    int one[1] = {7};
    int z;
    遍 z 于 one { }
    printf("empty=%d\n", z);
    if (z != 7) return 5;

    printf("PASS\n");
    return 0;
}
