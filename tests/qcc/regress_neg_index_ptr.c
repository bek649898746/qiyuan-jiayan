// @EXPECTED exit:0
// @EXPECTED out:ok
// regress: 负索引指针运算 p[-1] / *(p-1) — 32 位 -1 索引加到 64 位指针须符号扩展
//   (fix 2026-08-19: 原 mov %eax,%r11d 零扩展 → p + 0xFFFFFFFF → 指针溢出 SEGV
//    packed-backend find_start/find_end_of_record 的 p[-1] 扫描崩)
#include <stdio.h>

int main(void)
{
    char buf[8] = "hello";
    char *p = buf + 1;
    if (p[-1] != 'h') { printf("p[-1] fail %d\n", p[-1]); return 1; }
    if (*(p - 1) != 'h') { printf("*(p-1) fail\n"); return 2; }
    if (p[-1] != buf[0]) { printf("eq fail\n"); return 3; }
    /* 回退扫描循环 (find_start_of_record 形状) */
    char *q = buf + 5;
    while (q > buf && q[-1] != '\n')
        q--;
    if (q != buf) { printf("scan fail\n"); return 4; }
    printf("ok\n");
    return 0;
}
