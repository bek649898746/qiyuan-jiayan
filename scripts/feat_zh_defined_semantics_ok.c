/* feat_zh_defined_semantics_ok.c — defined()/#ifdef 只认宏, 不认函数/变量 (P0-6 根治 2026-09-14)
   病灶: #93 内联 特性 把 `inline 类型 名(参数){返 expr;}` 注册进 函数宏表 ⟹ 邻细()/外合() 误判"已定义"
        ⟹ `defined(名)` / `#ifdef 名` 对**普通 inline 函数**判真
        (git 的 gettext.h L12 `#if defined(_) || defined(Q_)` 误报 #错误, 打死 git 全部 .c)。
   实测矩阵(改前): 函数**定义**(带 inline) -> defined 真; 函数声明/变量/typedef/struct/enum/裸 -> 假。
   修法: 独立标记表 `内联宏标[]`, 邻细() 跳过标记条目 —— 保留 #93 的展开能力, 只关掉它对"是否已定义"判定的污染。
   本锚: 五种"非宏"声明 + 一个真宏 对照组，双向断言。 */
// @EXPECTED out:out=11
// @EXPECTED exit:0
#include <stdio.h>

static inline int fn_inline(void) { return 1; }
static int fn_plain(void) { return 2; }
static int var_x = 3;
typedef int td_y;
struct st_z { int a; };

#ifdef fn_inline
#error "fn_inline 被误判 已定义"
#endif
#if defined(fn_plain)
#error "fn_plain 被误判 已定义"
#endif
#if defined(var_x)
#error "var_x 被误判 已定义"
#endif
#if defined(td_y)
#error "td_y 被误判 已定义"
#endif
#if defined(st_z)
#error "st_z 被误判 已定义"
#endif

#define REAL_MACRO 5
#if !defined(REAL_MACRO)
#error "REAL_MACRO 应 已定义 (真宏 不能被误杀)"
#endif

int main(void) {
    printf("out=%d\n", fn_inline() + fn_plain() + var_x + REAL_MACRO);
    return 0;
}
