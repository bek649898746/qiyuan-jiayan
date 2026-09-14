/* feat_c_inline_struct_array_init_ok.c — 函数内 **内联定义** 结构体变量/数组 的 花括初始化器 (A78/A79/A80/A81 根治 2026-09-14)
   病灶 (一族 五个洞, 全部 同型: 同一语义 的 多条 平行 路径, 改一漏一):
     声明 结构体变量 在 函数内 有 **三条 平行 路径**:
       ① L8665 `结构体 [`已注册标签] 变量[维度]` (局部 声明)  ── 基准 (正确)
       ② L8887 `结构体 { ... } 变量` (**内联定义** + 实例, 词位 落 `{`)  ── 漏 4 项
       ③ L9151 已注册标签 但 词位 形态 落 该分支            ── 同漏
     漏 的 四项:
       A78: 花括初始化 = 整数组 元素级 已触 (`尾阵触(名,-1)`) —— 只在 ① 有 ⇒ 函数内
            内联定义 结构体数组 花括 后 **常数下标** 读 误 A4001 (git `commit.c` `bufs`);
            变量下标 因 动态下标 降 不追踪 而 侥幸通过 ⇒ 只有 常数下标 暴露.
            修: `左升` (整数组 花括初始化器 生成器, 自带 memset 零填) 深度 0 入口
                **一处收口** `尾阵触((字*)(节名表+元), -1)`.
       A79: `结构体 X *字段` 的 **指针标记** (`原进/符肆/内联/主挂/高贰`) —— ②③ 只算 尺寸 8
            不设 原进 ⇒ 字段是否指=0 ⇒ 赋值 被当 **结构体值** ⇒ 生成 「从 表达式值 当 地址
            拷 结构体大小 字节」 (空指针 0 ⇒ 地址 0 ⇒ 0xC0000005); 对照 顶层 L17376 (t44).
            实测: `struct trio { struct strbuf *p; int x; } bufs[2] = {{0,7},{0,9}};` (完全 不读)
            产物 崩; `struct strbuf` ≤ 8 字节 时 崩 不了 (走 8 字节 标量路径) ⇒ 被指类型 > 8 才暴露.
       A80: 数组判定 用 `空玖 || 扫描计数 > 1`, **漏 定长 单元素数组 `[1]`** ⇒ 走 标量 值布;
            且 末午/尾阵登 用 `扫描计数 > 1` ⇒ `struct X a[1]` 当 标量 注册 (数组尺寸 丢).
            修: 新增 **`是否数组`** 标记 (对齐 ① 的 `全退`), 五处 判据 统一.
       A80b/A80c: 维度解析 只认 `常数` token ⇒ `struct X a[N]` (N = 宏) 扫描计数 恒 1;
            且 未定长 推断 `词位 += 1` (应 += 2, 同 ① 的 P0-11-C) ⇒ 匿名结构体 空维度数组
            尺寸 恒 1 (编译器 自身 `计浅一[]` 40 个 汇编寄存器名 只登记 1 个 —— 静默 既有缺陷).
       A81: 主声明 带初始化 漏 `未初始化移除` (① 有, ②③ 漏) ⇒ 函数内 `构 X {...} v = {...};`
            之后 读 报 A4001 假阳.
   本锚: ① 内联定义 数组 (定长 + 空维度 + 匿名) 花括 → 常数/变量下标 读 (标量/指针字段);
         ② 内联定义 **标量** 结构体 花括 (A81); ③ 匿名结构体 空维度数组 元素数 推断 (A80c). */
// @EXPECTED out:7 9
// @EXPECTED out:7 9
// @EXPECTED out:1
// @EXPECTED out:2
// @EXPECTED out:3 4
// @EXPECTED out:5 1
// @EXPECTED out:3 c
// @EXPECTED exit:0
#include <stdio.h>

struct strbuf { int len; char *buf; };
struct point { int a; int b; };

int main(void)
{
    struct strbuf s1 = { 1, 0 };
    struct strbuf s2 = { 2, 0 };
    struct trio { struct strbuf *p; int *q; int x; } bufs[2] = {
        { &s1, 0, 7 },
        { &s2, 0, 9 },
    };
    struct quad { struct strbuf *p; int x; } one = { &s1, 5 };
    struct { const char *s; int v; } tab[] = {
        {"a", 0}, {"b", 1}, {"c", 2},
    };
    struct point pts[] = {
        { 3, 4 },
    };
    int i;

    printf("%d %d\n", bufs[0].x, bufs[1].x);
    for (i = 0; i < 2; i++)
        printf("%d%s", bufs[i].x, i == 1 ? "\n" : " ");
    printf("%d\n", bufs[0].q == 0);
    printf("%d\n", bufs[1].p->len);
    printf("%d %d\n", pts[0].a, pts[0].b);
    printf("%d %d\n", one.x, one.p->len);
    printf("%d %s\n", (int)(sizeof(tab) / sizeof((tab)[0])), tab[2].s);
    return 0;
}
