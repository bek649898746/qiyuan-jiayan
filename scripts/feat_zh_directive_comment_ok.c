// @EXPECTED out:7
/* C8 (2026-09-16): 条件指令行尾的跨行块注释 — 「跳过 directive 行」循环原只跳到行尾,
   块注释从第二物理行起被当作源码, 结束符永不被识别, 注释永不闭合
   ==> 整个翻译单元被静默丢弃 (rc=0, 目标码 0 字节, 无任何报错)。

   病灶: qcc_zh.jy 9 处同族循环 (原从 / 行申 / 词法 三个扫描器副本),
   形如「循环 (串值[计] && 串值[计] != 换行) { ... 否 计++; }」只跳行尾。
   C 语义: 注释在预处理之前化为空白 ==> 这些循环必须同时识别块注释开始符并跳到结束符。

   本锚: 三个条件分支各带一个跨行块注释, 运行必须输出 7。
   修复前: 整个文件被丢弃 (无产物 / 无输出); 修复后: 正常。 */
#include <stdio.h>

#ifdef __QCC_C8_NEVER_DEFINED__  /* this block comment deliberately spans
   two physical lines -- exactly the C8 trigger shape */
int never_compiled(void) { return 1; }
#else  /* another spanning
   comment right here */
int chosen(void) { return 7; }
#endif  /* and one more
   spanning comment to close the set */

int main(void) {
    printf("%d\n", chosen());
    return 0;
}
/* @EXPECTED out:7 */
