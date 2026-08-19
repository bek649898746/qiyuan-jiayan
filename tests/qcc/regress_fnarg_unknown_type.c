/* 复现: 单参数函数 + typedef 参数类型 (mode_t=unsigned short) —
   验证参数绑定与 S_ISREG 判定是否被编坏。
   两个已知失败形态:
   (a) mode_t 未注册 (系统头跳过): 幻影参数占 rcx → 真参数绑 rdx → 读垃圾
   (b) mode_t 注册为 unsigned short: 判定 (m & 0xF000)==0x8000 被折叠成假
   @EXPECTED
   exit: 0
   out: mode=33174 (0x81B6)
   */
#include <stdio.h>

typedef unsigned short mode_t;

static mode_t mode_native_to_git(mode_t native_mode)
{
    mode_t perm_bits = native_mode & 07777;
    if ((native_mode & 0xF000) == 0x8000) return 0x8000 | perm_bits;
    if ((native_mode & 0xF000) == 0x4000) return 0x4000 | perm_bits;
    return perm_bits;
}

int main(void)
{
    mode_t st_mode = 0x81B6;  /* S_IFREG | 0644 */
    st_mode = mode_native_to_git(st_mode);
    printf("mode=%u (expect 33174)\n", (unsigned)st_mode);
    if (st_mode == 0x81B6) return 0;
    return 1;
}
