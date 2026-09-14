/* feat_c_extern_funcptr_ok.c — 文件级 extern 函数指针变量 的 声明 / 定义 / 调用 (A95 + A95b 根治 2026-09-14)
   病灶: 顶层 extern 分支 原 **不认** `extern void (*名字)(参数);` 形态 ——
         型词循环 消费 `void` 后 词位 停在 `(`, 三个分支 (结构词 / 枚词 / 「词变量 + 左括」函数声明) 全不命中,
         前厚循环 的 `循环 (乘)` 也 不命中 (`(` 非 `*`) ⇒ 整条 声明 被 「跳到分号」 静默跳过
         ⇒ 名字 未登记 ⇒ 调用处 报方 A1001「未声明函数」(实测 git `packfile.h` L66
         `extern void (*report_garbage)(unsigned seen_bits, const char *path);` + `path.c` L383 调用).
   修法: extern 分支 补 该形态 (对齐 **非-extern 文件级路径 L18185**): 跳过 `( *` / 名字 / `)` / 参数类型列表,
         再 `变量静态(名, 4)` + `数组元素尺寸 = 8` (函数指针 = 8 字节; 双精度返回 再 标 指浮).
         ⚠️ 第一版 用 `半一` (外部符号) 登记 —— 实测 调用侧 **不认** (最小复现 p6 仍 A1001) ⇒ 改 与 L18185 完全一致的 `变量静态`.
   本锚: extern 声明 + 本文件定义 + 赋值 + 调用 + 输出 (回归保护锚 —— 该路径 旧种子 亦通过, 故 无分辨力;
         分辨力 由 git 批量 承担: `path.c` 由 FAIL 转 PASS, 222 -> 227/231). */
// @EXPECTED out:7 abc
// @EXPECTED exit:0
#include <stdio.h>

static int calls = 0;

static void real_handler(int bits, const char *path)
{
    calls++;
    printf("%d %s\n", bits * 7, path);
}

extern void (*report_garbage)(int bits, const char *path);
void (*report_garbage)(int bits, const char *path);

int main(void)
{
    report_garbage = real_handler;
    report_garbage(1, "abc");
    return calls == 1 ? 0 : 1;
}
