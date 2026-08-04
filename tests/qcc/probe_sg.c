/* probe_sg.c — static struct 数组综合测试: 写入 + char数组字段 */
static struct { char name[32]; int val; } tbl[8];
static int g_n;

int main(void) {
    int i;
    g_n = 2;
    tbl[0].val = 10;
    tbl[1].val = 20;
    tbl[g_n].val = 30;
    if (tbl[0].val != 10) return 1;
    if (tbl[1].val != 20) return 2;
    if (tbl[2].val != 30) return 3;
    /* char 数组字段: 写字符 + 读回 */
    tbl[0].name[0] = 'X';
    tbl[0].name[1] = 'Y';
    if (tbl[0].name[0] != 'X') return 4;
    if (tbl[0].name[1] != 'Y') return 5;
    /* 动态下标 */
    for (i = 0; i < 8; i = i + 1) tbl[i].val = i * 10;
    if (tbl[7].val != 70) return 6;
    if (tbl[3].val != 30) return 7;
    return 0;
}
