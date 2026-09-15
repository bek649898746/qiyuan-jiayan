/* A2 (2026-09-16): 宏体 字符串/字符 字面量 与 注释 内的标识符, 不得 被 实参 替换 —— 语料闸根治锚。
   病灶: 副组() 展开函数宏体时, 逐字节裸扫 体[], 遇 标识符 即与参数名比较:
         #定义 F(a) 'a'   调用 F(9)  → 体内 'a' 的 a 被当参数 → 得 '9'  (gcc: 'a')
         #定义 H(z) "z"   调用 H(Q)  → 得 "Q"                        (gcc: "z")
   后果: 实参 含 引号 时 (如 git `OPT__FORCE(var, h, f)` 体 含 'f', 实参 为 PARSE_OPT_NOCOMPLETE)
         体内 'f' 变成 'PARSE_OPT_NOCOMPLETE' → 词法 把 'P' 读成字符常量 → 后续 括号 失配
         → git 11 个 builtin/*.c 整文件 编不出 (`[ERR] 括号不匹配 '(' 长简 '}'`)。
   根治: 宏体→暂存 构建 (3 处 循环) 增加 字符串/字符字面量 与 注释 整体复制, 不参与 参数替换。
   本锚钉: 字符字面量 / 字符串字面量 / sizeof 串字面量 / 注释内参数名 / git 宏形态 五种情形, 与 gcc 逐字节一致。 */
#include <stdio.h>

#define LIT(a) ('a')
#define STR(z) "z"
#define MIX(k) (sizeof("k") + 'k')
#define CMT(a) (a) /* it's a comment with a */ + (a)
#define OPTG(s, l, v, h, f) { .type = 1, .short_name = (s), .flags = 2 | (f) }
#define OPTF(var, h, f) OPTG('f', "force", (var), (h), (f))

struct opt { int type; int short_name; int flags; };

int main(void) {
    char c = LIT(Q);
    const char *s = STR(Q);
    int n = MIX(Q);
    int m = CMT(20);
    struct opt o = OPTF(0, "x", 4);
    printf("c=%d s=%s n=%d m=%d t=%d sh=%d fl=%d\n", c, s, n, m, o.type, o.short_name, o.flags);
    return 0;
}
/* @EXPECTED out:c=97 s=z n=109 m=40 t=1 sh=102 fl=6 */
