// @EXPECTED exit:0
// @EXPECTED out:ok=1
// bug#25: 逗号续行 struct 字段缺类型索引 — struct list_head { struct list_head *next, *prev; }
// 的续行字段 *prev 只继承大小(8) 未继承类型 → st_field_ty_idx(list_head,"prev")=-1
// → mem_addr 链 head.prev->next 外层 sub_si<0 失败 → 嵌套成员存储被丢弃 + RHS push 无配对
// → 函数 epilogue ret 弹错地址 (旧 rbp) → SEGV (git init chdir_notify_register list_add_tail 崩)
// 修复: sty_persist — ST 分支设类型索引, 逗号续行字段继承, ; 重置
#include <stdio.h>

struct list_head { struct list_head *next, *prev; };

struct node { struct list_head list; };

static struct list_head head = { &head, &head };

static void list_add_tail(struct list_head *newp, struct list_head *h)
{
    h->prev->next = newp;
    newp->next = h;
    newp->prev = h->prev;
    h->prev = newp;
}

int main(void) {
    struct node n;
    n.list.next = 0;
    n.list.prev = 0;
    list_add_tail(&n.list, &head);
    int ok = (head.next == &n.list && head.prev == &n.list &&
              n.list.next == &head && n.list.prev == &head);
    printf("ok=%d\n", ok);
    return ok ? 0 : 1;
}
