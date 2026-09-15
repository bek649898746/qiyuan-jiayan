// @EXPECTED out:PASS
#include <stdio.h>
#include <string.h>

/* G1-1 / A19 回归 锚: **嵌套 成员链** 的 双精度 字段 读
 *   缺陷 (2026-09-11 修): 两 处 `浮取 xmm0, [r0]` 只 用 汇发 打 汇编文本, **漏 了 机器码 编码**
 *   (其余 9 处 同形 站点 都 有 `发字节(0xF2); 发字节(0x0F); 发字节(0x10); 模字节(0,0,0);`)。
 *   → 取值 指令 根本 没 发射, 消费端 读 到 xmm0 里 的 **残留值**: 表现 为 0.0 /
 *     别人 变量 的 值 / 「第 一 次 对, 之后 全 错」。
 *   两 处: 嵌套链 64 位 分支 + 变量基 64 位 分支 (c->r 简单箭头)。
 * 本 锚 覆盖: ① 嵌套链 多次 使用 ② 经 指针 ③ 同型 双 全局 (原 会 读成 别人 的 值)
 *   ④ 联合 内 嵌套 ⑤ 平级 字段 对照 ⑥ 箭头 链 ⑦ 结构体 值 传参 后的 嵌套读 */

typedef enum { T_INT, T_DBL, T_STR } Kind;
typedef struct { Kind kind; union { int i; double d; const char *s; } val; } Value;

typedef struct { int i; double d; } Inner;
typedef struct { Inner in; } Outer;

struct Flat { double d; };

static Outer g;
static Outer h;
static Inner other;

static double f1(void) { return g.in.d + 0.0; }

int main(void) {
    /* ① 嵌套链 多次 使用 (原: 第 1 次 对, 之后 0.0) */
    Outer o;
    o.in.d = 2.5;
    o.in.i = 7;
    if (o.in.d + 0.0 < 2.49 || o.in.d + 0.0 > 2.51) return 1;
    if (o.in.d + 0.0 < 2.49 || o.in.d + 0.0 > 2.51) return 2;
    if (o.in.d * 2.0 < 4.99 || o.in.d * 2.0 > 5.01) return 3;
    if (o.in.i != 7) return 4;

    /* ② 经 指针 */
    Outer *po = &o;
    if (po->in.d + 0.0 < 2.49 || po->in.d + 0.0 > 2.51) return 5;

    /* ③ 同型 双 全局 (原: g 读成 h 的 值) */
    g.in.d = 3.5;
    h.in.d = 4.5;
    if (g.in.d < 3.49 || g.in.d > 3.51) return 6;
    if (h.in.d < 4.49 || h.in.d > 4.51) return 7;
    if (f1() < 3.49 || f1() > 3.51) return 8;

    /* ④ 「含 同 名 字段 的 另 一 变量」 (原: g.in.d 读成 other.d) */
    other.d = 9.5;
    if (g.in.d < 3.49 || g.in.d > 3.51) return 9;
    if (other.d < 9.49 || other.d > 9.51) return 10;

    /* ⑤ 平级 字段 对照 */
    struct Flat s;
    s.d = 5.5;
    if (s.d + 0.0 < 5.49 || s.d + 0.0 > 5.51) return 11;
    if (s.d + 0.0 < 5.49 || s.d + 0.0 > 5.51) return 12;

    /* ⑥ 联合 内 嵌套 (原 A19 的 原始 形态) */
    Value v;
    v.kind = T_DBL;
    v.val.d = 6.25;
    if (v.val.d + 0.0 < 6.24 || v.val.d + 0.0 > 6.26) return 13;
    if (v.val.d + 0.0 < 6.24 || v.val.d + 0.0 > 6.26) return 14;
    v.kind = T_INT;
    v.val.i = 42;
    if (v.val.i != 42) return 15;

    /* ⑦ 变参 位 与 实参 位 */
    printf("chain=%.4f ptr=%.4f g=%.4f h=%.4f flat=%.4f union=%.4f\n",
           o.in.d, po->in.d, g.in.d, h.in.d, s.d, 6.25);

    printf("PASS\n");
    return 0;
}
