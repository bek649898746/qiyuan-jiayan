// @EXPECTED out:ofst buf=8 a=8 b=16 addr buf=8 a=8 b=16
/* A55 锚 (2026-09-14): `offsetof` **嵌套 member-designator** (C99 7.17 允许 `offsetof(T, a.b.c)`)。
   病灶: 编译器内置 offsetof 处理 **只 读 一个 字段名** 就 跳 到 `)`, 后续 `.字段` 链 **静默 跳过** ⟹
     返回 的 是 最外层 字段 的 偏移, 内层 偏移 全 丢。
   铁证 (决定性地 分离 了两个 假说): `offsetof(struct s1, buf.b)` 甲言 **8** / gcc **16**;
     而 **地址差** `(char*)&v.buf.b - (char*)&v` 甲言 **16** 与 gcc 一致 ⟹ **布局没问题**,
     错 的 只是 **编译期 那个 计算** (`字段序` 只 给 下标, `字段[].毛利` 才是 该字段的 结构体类型序)。
   修法: 循环 消费 `.字段` 链, 逐层 累加偏移 + 沿 `毛利` 下钻; 类型序 断开 或 非 `.` 即 停。
   发现路径: 层3 扫锚 (tests/regress_union_nested.c 等 不一致 项 的 追查 中 分离 出来)。
   证据: 输出 与 gcc 逐字节 一致 (offsetof 与 地址差 两路 都 对上)。 */
#include <stdio.h>
#include <stddef.h>

struct Inner { int a; double b; };
struct s1 { int x; struct { int a; double b; } buf; };   /* 内联 匿名 struct 成员 */
struct s2 { int x; struct Inner buf; };                   /* 具名 类型 成员 (对照) */

int main(void) {
    struct s1 v;
    v.buf.b = 1.0;
    long ab = (long)((char *)&v.buf.b - (char *)&v);
    printf("ofst buf=%d a=%d b=%d addr buf=%ld a=%ld b=%ld\n",
        (int)offsetof(struct s1, buf), (int)offsetof(struct s1, buf.a), (int)offsetof(struct s1, buf.b),
        (long)((char *)&v.buf - (char *)&v), (long)((char *)&v.buf.a - (char *)&v), ab);
    printf("s2 buf=%d b=%d\n", (int)offsetof(struct s2, buf), (int)offsetof(struct s2, buf.b));
    return 0;
}
