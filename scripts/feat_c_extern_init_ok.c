/* feat_c_extern_init_ok.c — `extern <类型> 变量 = 初始化;` = 定义 (A108 根治 2026-09-14)
   病灶: 顶层 `extern` 分支 只 登记 **外部符号** (`半一`), **不处理 初始化器** ——
         实测 `extern struct pair { int a; int b; } v = { 7, 9 };` 甲言 打印 `4347069 4347069` (垃圾)
         而 gcc 打印 `7 9` (初始化器 被 静默 跳过).
   修法: C 语义下 **带初始化的 extern 就是 定义** ⇒ 前瞻 判定 `extern <类型> 名 [维度] = ` 形态,
         命中则 **跳过 `外部` 词 + 续**, 让 主循环 以 普通顶层声明路径 重新解析
         (那条路径 A75 系列 已 完整支持 内联定义 + 花括初始化).
         ⚠️ 首版 (A108) 前瞻 未跳过 **结构体体 `{ ... }`** ⇒ 判定恒 0 ⇒ 未生效; A108b 补上.
   本锚: extern + 内联定义 + 花括初始化 + 读回. */
// @EXPECTED out:7 9
// @EXPECTED exit:0
#include <stdio.h>

extern struct pair { int a; int b; } v = { 7, 9 };

int main(void)
{
    printf("%d %d\n", v.a, v.b);
    return 0;
}
