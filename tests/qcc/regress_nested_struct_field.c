// @EXPECTED exit:0
// @EXPECTED out:OK
/* 回归 2026-08-18: 嵌套 struct 成员后的标量字段读取 — 原 struct 字段后的 ';' 不重置字段类型状态
   (sty_persist 残留) → 后续 int 字段继承 struct 类型 (fsz=16 fty=struct) → 读取走 fsz>8 分支 lea 取地址
   → git lockfile/tempfile 结构全崩。修复: struct 字段后 ';' 重置 fsz/frow/sty_persist。 */
#include <stdio.h>

struct lst { struct lst *next, *prev; };
struct T { struct lst list; int x; };

int f(void) {
    struct T t;
    t.list.next = 0;
    t.list.prev = 0;
    t.x = 5;
    return t.x;
}

int main(void) {
    int r = f();
    if (r != 5) { printf("FAIL %d\n", r); return 1; }
    printf("OK\n");
    return 0;
}
