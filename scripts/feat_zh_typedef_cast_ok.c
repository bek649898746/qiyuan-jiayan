// @EXPECTED out:3 3 4 3 3 6 4
#include <stdio.h>

/* ORD-20261104-103 锚：typedef 类型名 在 多次 同类型 局部声明 后 仍 必须 可作 强转
 *
 * 病灶：语句里 某处 把 `size_t` 当 标识符 表达式 解析 ⟹ R610「隐式全局 预登记」给
 *   `size_t` 建了 **对象条目** ⟹ `型定判断` 的 C4-a 遮蔽判据（同名对象 遮蔽 typedef）
 *   从此 把 `size_t` 判成「不再是 类型名」⟹ 后续 `(size_t)` 强转 全部 退化成 括号表达式
 *   ⟹ 误报 A1004「返回 之后 必须 是 分号」(实测 lua lstrlib.c / _mc6.c)。
 *
 * 修法：`预登全局` 拒绝 typedef 名（C 语义：typedef 名 不能被 隐式声明 为 对象）⟹ 污染源 消失。
 *
 * 灵敏度：旧编译器 A1004（tok=pos）；新编译器 编过 且 输出 `3 3 4 3 3 6 4`。
 * 覆盖：return 强转 / if 分支赋值 / 双层括号 / for 初始化 / 三目 / 乘法 / 逗号声明。
 */

typedef long lua_Integer;
typedef unsigned long size_t;

static size_t s1 (lua_Integer pos) { return (size_t)pos; }
static size_t s2 (lua_Integer pos) { size_t r; if (pos > 0) r = (size_t)pos; else r = 0; return r; }
static size_t s3 (lua_Integer pos) { return ((size_t)pos) + 1; }
static size_t s4 (lua_Integer pos) { size_t r = 0; for (size_t i = 0; i < (size_t)pos; i++) r += i; return r; }
static size_t s5 (lua_Integer pos) { size_t r = (size_t)(pos > 0 ? pos : 0); return r; }
static size_t s6 (lua_Integer pos) { return (size_t)(pos) * 2; }
static size_t s7 (lua_Integer pos) { size_t a = 1, b = (size_t)pos; return a + b; }

int main(void) {
    printf("%lu %lu %lu %lu %lu %lu %lu\n",
           (unsigned long)s1(3), (unsigned long)s2(3), (unsigned long)s3(3),
           (unsigned long)s4(3), (unsigned long)s5(3), (unsigned long)s6(3), (unsigned long)s7(3));
    return 0;
}
