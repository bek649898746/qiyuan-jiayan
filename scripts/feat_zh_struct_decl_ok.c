// @EXPECTED out:PASS 结构体声明形态
#include <stdio.h>

/* A16 (2026-09-11) 正 锚: 结构体 值/指针/数组/逗号 声明 各 形态 仍 正常
 *   (与 feat_zh_struct_unknown_tag_neg 配对: 诊断 只 拦 未知 标签 的 值 声明, 不 误伤 合法 形态)
 *   覆盖: ① typedef 名 值 声明 ② 指针 声明 + 箭头 ③ 数组 声明
 *         ④ 逗号 多 声明 ⑤ 英文 struct 标签 形态 ⑥ 未知名 指针 (C 式 不完整类型, 保留 不 报错) */

typedef struct { int 甲; int 乙; } 点;
struct 树 { int v; struct 树 *左; };

int main(void) {
    点 p;
    p.甲 = 7;
    p.乙 = 8;

    构 点 a = p;      /* ① */
    构 点 *q = &p;    /* ② */
    构 点 arr[2];     /* ③ */
    arr[0] = p;
    arr[1] = p;
    构 点 b, c;       /* ④ */
    b = p;
    c = p;
    struct 树 t;      /* ⑤ */
    t.v = 5;
    t.左 = 0;
    构 未知标签 *未知指针 = 0; /* ⑥ 不完整类型 指针: 保留 原 行为 (不 报 本节 诊断) */

    printf("a=%d %d q=%d arr=%d %d b=%d c=%d t=%d\n", a.甲, a.乙, q->甲, arr[0].甲, arr[1].乙, b.甲, c.乙, t.v);
    if (a.甲 != 7 || a.乙 != 8) return 1;
    if (q->甲 != 7) return 2;
    if (arr[0].甲 != 7 || arr[1].乙 != 8) return 3;
    if (b.甲 != 7 || c.乙 != 8) return 4;
    if (t.v != 5) return 5;
    if (未知指针 != 0) return 6;
    printf("PASS 结构体声明形态\n");
    return 0;
}
