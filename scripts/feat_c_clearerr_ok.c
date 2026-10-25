// @EXPECTED out:ok
/* A1005 (2026-09-16): 内置/导入表 豁免表 (后聚[]) 缺 clearerr —— 语料闸 (git pack-objects) 根治锚。
   病灶: `builtin/pack-objects.c` 调 `clearerr(stdin)` ⟹ A1001 未声明函数 (C99 废隐式声明后,
         不在 原型表/内置豁免表 里的 标准库 函数 一律 拒收)。clearerr 是 msvcrt 导出的 标准 stdio 函数,
         只是 豁免表 漏登记 ⟹ git 11 个 builtin 里 唯一 剩的 一个。
   修法: 后聚[] (内置/导入表 函数 豁免表) 补 "clearerr" —— 最小必要 (豁免表 加多了 会 掩盖 真实拼写错)。
   本锚钉: clearerr(stdin) 可 编译 可 链接 可 运行, 与 gcc 同行为。 */
#include <stdio.h>

int main(void) {
    clearerr(stdin);
    printf("ok\n");
    return 0;
}
/* @EXPECTED out:ok */
