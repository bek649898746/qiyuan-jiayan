// @EXPECTED exit:0
/* probe_pp.c — 后置自增/自减测试 */
int main(void) {
    int i = 0;
    int r;
    int s;
    i++;
    if (i != 1) return 1;
    r = i++;
    if (r != 1) return 2;
    if (i != 2) return 3;
    i--;
    if (i != 1) return 4;
    /* for 步进用 i++ */
    s = 0;
    for (i = 0; i < 5; i++) s = s + i;
    if (s != 10) return 5;
    /* 全局自增 */
    return 0;
}
