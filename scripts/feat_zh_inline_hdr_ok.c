/* feat_zh_inline_hdr_ok.c — #93 内联 误注册 标准 C「跨行花括号」inline 函数 (P0-7 根治 2026-09-14)
   病灶: 净集() 的 #93 分支 (英文 inline) 提取宏体时, 签名 与 花括号**跨行**也照样注册,
        且 无条件 跳过换行 → 把 **下一行** 当宏体 (体 以 '{' 开头, 残破)。
   引爆: 副组() 的 `#define 名(` 激活是**文本级判断**, 不看 #ifdef/#if 条件 ⟹
        条件块内的 `#define` (如 compat_prelude.h 在 #ifdef QCC_OBSTACK_GETTEXT_STUB 里的
        `#define _(s) (s)`) 也会把同名条目激活 → 调用点展开残破宏体 → 括号配对失败「缺右括号」。
   实测: git 根目录 20 个真实源文件 里 14 个 全卡这一条 (gettext.h 的 `static inline ... _(...)`)。
   修法: 花括号 不在签名行内 则不注册 (甲言 内联宏 的定义形态 本就是单行)。
   本锚: 条件块内的同名宏 + 跨行花括号函数 + 同行花括号对照。
   复现要点: `wrapped` 的宏定义在 **永不成立的条件块**里 (gcc 侧不定义), 甲言侧 文本级激活。 */
// @EXPECTED out:out=65,66,67
// @EXPECTED exit:0
#include <stdio.h>

#ifdef NEVER_ENABLED_BY_GCC
#define wrapped(s) (s)
#endif

/* 跨行花括号: 修复前 被 #93 误注册为宏 (体残破) + 条件块 `#define` 激活 → 展开 → 括号不匹配 */
static inline int wrapped(const char *m)
{
	return m[0];
}

/* 同行花括号: 甲言 内联宏 定义形态, 必须保持工作 */
static inline int plain(const char *m) { return m[0] + 1; }

int main(void) {
	printf("out=%d,%d,%d\n", wrapped("A"), plain("A"), wrapped("C"));
	return 0;
}
