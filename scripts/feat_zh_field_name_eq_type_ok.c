/* C4-b 锚 (2026-10-15 虾米 · 台账 C4-b「A9001 字段不存在」最大簇根治): **字段名 == 其类型 typedef 名**.

   病灶: `typedef struct 标签 { … } 别名;` 的 字段循环 里, 「字段类型 = typedef 名」分支
     **只 `词位++` 消费 类型名, 不 消费 其后的 `*`** ⟹ 指针 星号 落到 **下一轮 迭代** 消费
     (而 下一轮 `实丁` 已复位 0) ⟹ 再 下一轮 又见 **字段名**, 若 字段名 恰是 typedef 名
     (`用户 *用户;` / redis 原式 `user *user;`) 则 该轮 typedef 判据 (`!正庚`) 成立
     ⟹ 字段名 被 **再当 一次 类型** 吃掉 ⟹ **字段 从未 登记**.
     QCC_DUMP=types 铁证: `typedef struct client { user *user; int flags; } client;` 的
     `client` **只有 flags**, `user` 整条 丢失 ⟹ 语料 里 `c->user` 报
     `A9001 类型 'client' 无字段 'user'` (台账 C4-b = 最大 簇, redis 25 处 / 33 文件).

   修: 本分支 与 **匿名体 同族分支** (内含 `循环 (词型表[词位] == 乘)`) 对齐 —— 同轮 消费 星号,
     并 以 `下失C4b` 取 星号前 的 元素大小 (typedef 尺寸) 供 指针字段 登记 主挂/符肆.
     同族不对称 是 本项目 已登记的 一类 反复病灶 (「同一 语义 两套 口径」).

   灵敏度 (旧编译器 = F80B5DA5): 本锚 **rc=1**, 报 `A9001 类型 'client' 无字段 'user'` ⟹ 旧编译器必红.
   账目: `grok-build\_DEBT_INDEX.md` C4-b (族3 `A9001` 字段不存在; redis `client.user`). */
// @EXPECTED out:U=7 F=5 A=9 B=1
// @EXPECTED exit:0
#include <stdio.h>

typedef struct user { int a; } user;
typedef struct client { user *user; int flags; } client;

/* 逗号 续声明符 亦覆盖 (`user *user, *other;` 形态) */
typedef struct pair { user *user, *other; } pair;

int main(void) {
    user u;
    user u2;
    client c;
    pair p;
    u.a = 7;
    u2.a = 9;
    c.user = &u;
    c.flags = 5;
    p.user = &u2;
    p.other = 0;
    printf("U=%d ", c.user->a);
    printf("F=%d ", c.flags);
    printf("A=%d ", p.user->a);
    printf("B=%d", (int)(p.other == 0));
    return 0;
}
