#include <stdio.h>

/* 甲言 标准库种子 jyc_lib (2026-08-28 吃狗粮⑤, 真接入)
   甲言写的真库: 前置/后置 契约在真实程序里运行 (非测试模拟) — 用 -I 前置给用户程序:
     qcc -I scripts/jyc_lib.c user.c -o app.exe
   库函数全部带契约: 前置=入口检查 (编译期常量参数还会被编译器当场审判), 后置=出口检查 */

/* 字符串长度 (前置: 指针非空) */
int 字符串长度(const char *s) 前置 (s != 0) 后置 (返回 >= 0) {
    int n = 0;
    while (s[n] != 0) n++;
    return n;
}

/* 字符串拷贝 (前置: 源非空, 目标非空) */
void 字符串拷贝(char *dst, const char *src) 前置 (dst != 0) 前置 (src != 0) {
    int i = 0;
    while (src[i] != 0) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

/* 整数除法 (前置: 除数非零 — 编译器能当场抓 div_safe(10,0) 类常量违反) */
int 整数除法(int a, int b) 前置 (b != 0) 后置 (返回 == a / b) {
    return a / b;
}

/* 限幅 (前置: 区间合法 lo<=hi; 后置: 结果在区间内) */
int 限幅(int v, int lo, int hi) 前置 (lo <= hi) 后置 (返回 >= lo) 后置 (返回 <= hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* 数组求和 (前置: 指针非空, 长度非负) */
int 数组求和(const int *arr, int n) 前置 (arr != 0) 前置 (n >= 0) {
    int s = 0;
    for (int i = 0; i < n; i++) s += arr[i];
    return s;
}

/* 契约结构体: 二维点 */
struct 点 { int x; int y; };
契约 点;

/* 点距离平方 (后置: 非负) */
int 点距离平方(struct 点 p) 后置 (返回 >= 0) {
    return p.x * p.x + p.y * p.y;
}

/* 整数最大值 (后置: 返回是较大者) */
int 取大(int a, int b) 后置 (返回 >= a) 后置 (返回 >= b) {
    return a > b ? a : b;
}

/* === 声明先行吃狗粮 (2026-08-28 真接入⑦): 声明 API 骨架 → 实现后补 → 前向调用正常 (AI 渐进补全模式) === */
声明 字符串比较(const char *a, const char *b) 返回 int;   /* 声明骨架 (签名先行) */

/* 字符串相等: 前向调用 声明 的 字符串比较 (declare 提供签名, 定义在后) */
int 字符串相等(const char *a, const char *b) 前置 (a != 0) 前置 (b != 0) {
    return 字符串比较(a, b) == 0;
}

/* 实现后补 (与 声明 签名一致) */
int 字符串比较(const char *a, const char *b) {
    while (*a != 0 && *b != 0 && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* === 内存序吃狗粮 (2026-08-28 真接入⑧): 原子计数器 — 释放写/获取读 (并发正确性) === */
int g_原子计数 = 0;

void 原子增(void) {   /* 释放写: 递增结果对其他线程可见 */
    int v = 原子 读(&g_原子计数);
    原子 写(&g_原子计数, v + 1) 释放;
}

int 原子取(void) {    /* 获取读: 读到已释放的最新值 */
    return 原子 读(&g_原子计数) 获取;
}

/* === 覆写移动吃狗粮 (2026-08-28 真接入⑧): 自定义移动钩子 — 移入时调用 钩子(&dst,&src) 替代按位 === */
int g_移动日志 = 0;
void 移动钩子(int *dst, int *src) { g_移动日志 += *src; *dst = *src; *src = 0; }  /* 钩子: 记录源值, 搬移, 清源 */

int 带钩子移动(int v) {   /* 移入 走自定义钩子而非按位拷贝 */
    int src = v;
    int dst = 0;
    移入 dst, src 覆写 移动钩子;
    return dst;
}

/* === 机制层 (借用/移动/可空/错误union/RAII) — 真实库函数吃狗粮 (2026-08-28 真接入⑥) === */

/* 借用: 只读阶段 借用/放还 — 真实数组访问函数 */
int 取第(const int *arr, int n, int i) 前置 (arr != 0) 前置 (i >= 0) 前置 (i < n) {
    借用 arr;            /* 只读借用: 读 OK, 写会[ERR]/自动放还 */
    int v = arr[i];
    放还 arr;            /* 显式结束只读阶段 */
    return v;
}

/* 可空: 查找返回 0 表未找到 (可空指针), 调用方 解包 强制处理 */
int *查找值(int *arr, int n, int v) {
    for (int i = 0; i < n; i++) if (arr[i] == v) return &arr[i];
    return 0;            /* 可空: 0 = 空 */
}

/* 错误 union: 除或错 — b==0 显式返回错误码 1 (Go 式 error, AI 找根因) */
错误 int 除或错(int a, int b) {
    if (b == 0) { 错 1; return 1; }   /* 显式错误码 */
    return a / b;
}

/* 移动: 点移动 — 参数 p 的所有权转移给局部 q (移入 后 p 失效, Rust 式 move) */
struct 点 点移动(struct 点 p) {
    struct 点 q;
    移入 q, p;           /* p 移动给 q, 移后 p 失效 (再读[ERR]) */
    return q;
}

/* RAII 守卫: 析构函数 块退出自动调用 (LIFO 逆序) — 真实资源清理 (守卫把变量按值传给析构) */
int g_析构日志 = 0;
void 析构记录(int v) { g_析构日志 += v; }   /* 析构: 日志累加守卫变量值 (按值接收) */

int 守卫求和(void) {
    int a = 5;
    守卫 a, 析构记录;    /* 块退出 → 析构记录(&a) */
    int b = 10;
    守卫 b, 析构记录;    /* LIFO: b 先析构, 再 a */
    return a + b;        /* 15; 退出后 g_析构日志 = 10 + 5 = 15 */
}
