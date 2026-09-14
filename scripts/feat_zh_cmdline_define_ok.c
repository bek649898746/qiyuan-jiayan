/* feat_zh_cmdline_define_ok.c — 命令行 -D 宏注入 (P0-3 根治, 2026-09-14)
   病灶: -D 的宏 走 数值宏表 (宏加/终复), 而 现退一() 首句 终子() 把 宏数 清零
         ⟹ 命令行 宏 原地蒸发; 且 数值宏表 只供 #if 求值, 不参与 源码文本替换。
   覆盖: 分离形 -D NAME=VAL / 紧凑形 -DNAME=VAL (GCC 惯例) / #ifdef 判定 /
         #if 数值求值 / 源码内 标识符 文本替换 / 无 = 值 时 默认 1。
   神谕: gcc 同 参数 逐字节一致 (见 scratch_gl 对照记录)。 */
// @ARG -DCMDSEP=42
// @ARG -D CMDSPC=7
// @ARG -DCMDFLAG
// @EXPECTED out:ifdef=42,7,1
// @EXPECTED out:code=49
// @EXPECTED out:ifnum=ok
// @EXPECTED exit:0
#include <stdio.h>
int main(void) {
#ifdef CMDSEP
    printf("ifdef=%d,%d,%d\n", CMDSEP, CMDSPC, CMDFLAG);
#else
    printf("ifdef=UNDEF\n");
#endif
    printf("code=%d\n", CMDSEP + CMDSPC);
#if CMDSEP == 42 && CMDSPC == 7 && CMDFLAG == 1
    printf("ifnum=ok\n");
#else
    printf("ifnum=bad\n");
#endif
    return 0;
}
