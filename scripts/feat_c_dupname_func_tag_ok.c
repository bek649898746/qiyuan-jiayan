// @EXPECTED out:hit=1
/* C4 (2026-09-16): 函数名 与 结构体标签 **同名** 时, `名字(实参)->字段(...)` 必须 解析为
   **函数调用 + 成员访问** —— C 语义: 标签名 不能 单独 作 类型名, `结构体 X (y);` 非法,
   唯一 合法 的 括号 声明符 是 typedef 名 `T (y);`。
   病灶: 语句层 把 裸标签名 (词变量 && 查类型>=0) 当 类型声明 吞掉 ⟹ 后续 `(实参)->字段`
   另起 一条 语句 解析 ⟹ 成员 基 落成 实参 本身 ⟹ 误报 A9001 (git reftable/record.c L1163)。
   本锚: 同名函数 返回 vtable 指针, `->key` 经 函数指针 字段 调用, 运行时 必须 命中。 */
#include <stdio.h>

struct reftable_record { int type; };
struct strbuf { int len; };
struct reftable_record_vtable { void (*key)(const void *r, struct strbuf *d); };

static struct reftable_record_vtable vt;
static int hit = 0;

static void my_key(const void *r, struct strbuf *d) { (void)d; if (r) hit = 1; }

static struct reftable_record_vtable *reftable_record_vtable(struct reftable_record *rec) {
    (void)rec;
    return &vt;
}

int main(void) {
    struct reftable_record rec;
    rec.type = 42;
    vt.key = my_key;
    reftable_record_vtable(&rec)->key(&rec, 0);
    printf("hit=%d\n", hit);
    return 0;
}
/* @EXPECTED out:hit=1 */
