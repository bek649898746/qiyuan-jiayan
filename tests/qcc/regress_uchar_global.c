// @EXPECTED exit:0
// @EXPECTED out:1-2-3
// 验证: 宿主 qcc 编 unsigned char 全局数组 + 变量索引的步长
#include <stdio.h>

static unsigned char g_data[1024];

static void write_at(int off, int v) {
    g_data[off] = (unsigned char)v;
    g_data[off + 1] = (unsigned char)((v >> 8) & 0xff);
}

int main(void) {
    int i;
    for (i = 0; i < 16; i++) g_data[i] = 0;
    write_at(0, 0x0201);
    write_at(2, 0x0403);
    printf("%d-%d-%d\n", g_data[0], g_data[1], g_data[2]);
    return 0;
}
