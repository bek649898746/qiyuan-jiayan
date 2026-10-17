/* C4-a 锚 (2026-10-15 虾米 · 台账 C4-a「族1 崩溃」根治): **typedef 名的 遮蔽**.

   病灶: 型定判断() 只查 标降一 (typedef 名表), **完全 无视 遮蔽** ⟹ 形参/局部 与 typedef 同名时,
     该名 仍 被当 类型名 ⟹ `(list)->len` 的 `(list)` 被 误判 为 **转型 (list) expr** ⟹ 转型 解析
     吃掉 `(list)` 后 期待 操作数, 实得 `->len` ⟹ 原级() 落空 返 -1 ⟹ 下游 以 -1 索引 平行数组
     (表基址 + 4×(-1) = 页首 前 4 字节 = 未映射) ⟹ **编译器自身 0xC0000005** —— 即
     redis `adlist.c` / `redis-benchmark.c` 串行 可复现 崩溃 的 根因 (本锚 = 其 最小复现).

   修 (三件, 真结构 · 单一权威):
     ① 型定判断() 增 **遮蔽** 判据 (C11 6.2.1p4: 内层 同名 **对象** 声明 遮蔽 外层 typedef 名)
     ② **原型 形参 不 参与 遮蔽** (C11 6.2.1p7 原型作用域: 作用域 止 于 声明 末尾) —— 否则
        `void listRelease(list *list);` 的 形参 会 让 文件作用域 的 `list` 不再 是 类型名 ⟹
        后续 定义 的 **返回类型** `list *` 解析 落空 (实测 真回归: 块 [2,4] / [3,4] 组合 复现)
     ③ 遮蔽 判据 以 **块深度 (净丙)** 门控: 文件作用域 无 遮蔽 (同名 对象 与 typedef 名 并立
        = C 的 重复声明 错误) —— 否则 上一定义 的 形参条目 (甲言 函数出口 不回收形参) 会 泄漏为
        文件级 遮蔽 ⟹ 同上 真回归.

   灵敏度 (旧编译器 实测): 本锚 **崩 0xC0000005** (rc=3221225477) ⟹ 旧编译器必红.
   账目: `grok-build\_DEBT_INDEX.md` C4-a (族1 崩溃: redis `adlist.c` / `redis-benchmark.c`). */
// @EXPECTED out:SH=12 SH2=12 M=18
// @EXPECTED exit:0
#include <stdio.h>

typedef struct list {
    struct list *head;
    unsigned long len;
} list;

#define listLength(l) ((l)->len)

/* 形参 `list` 遮蔽 typedef 名 `list` ⟹ `(list)` 只能是 **括号表达式**, 不是 转型 */
unsigned long probe(list *list, unsigned long n) {
    list->len = n;
    return listLength(list) + ((list)->len);
}

/* 局部变量 遮蔽 typedef 名 (块内 声明 之后) */
unsigned long probe2(unsigned long n) {
    list *list = 0;
    unsigned long v = n;
    if (list == 0) return v * 2;
    return (list)->len;
}

int main(void) {
    list l;
    l.head = 0;
    l.len = 0;
    printf("SH=%d ", (int)probe(&l, 6));
    printf("SH2=%d ", (int)probe2(6));
    printf("M=%d", (int)(l.len + 12));
    return 0;
}
