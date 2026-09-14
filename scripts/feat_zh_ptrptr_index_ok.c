/* feat_zh_ptrptr_index_ok.c — 指针变量的多级下标 (A71 根治 2026-09-14)
   病灶: 指针变量作嵌套基时, 外层 [j] 既不加载指针(少一次解引用) 也不设步长(长卯=0)
         ⟹ 外层索引非 0 时地址全错; 而 p[1][0] 恰好对 (0×任何=0) 掩盖了缺陷。
   覆盖: 局部指针变量 / 形参 / 字符双指针 / 数组名对照 / 2D 数组对照 / 全局指针数组对照 / 单跳对照。
   注: char 元素用 %c 断言 —— 甲言 `(int)char` 转换 是 **另一** 既存缺陷 (argv 探针实测), 勿混入本站。 */
// @EXPECTED out:A=30,Z
// @EXPECTED out:B=20,Y
// @EXPECTED out:C=40,W
// @EXPECTED out:D=40,30
// @EXPECTED out:E=30,Z
// @EXPECTED out:F=4,6,2
// @EXPECTED out:G=72,33
// @EXPECTED exit:0
#include <stdio.h>
static int vals[4] = {10, 20, 30, 40};
static int *garr[2];
static int gvals[2] = {71, 72};

static int sum2(int **p, int i, int j) { return p[i][j]; }

int main(void) {
    int *arr[2];
    int **pp = arr;
    char *sa[2];
    char **sq = sa;
    int m[2][3];
    int row[3];
    int *rp = row;
    arr[0] = &vals[0];
    arr[1] = &vals[2];
    sa[0] = "XY";
    sa[1] = "ZW";
    m[0][0] = 1; m[0][1] = 2; m[0][2] = 3;
    m[1][0] = 4; m[1][1] = 5; m[1][2] = 6;
    row[0] = 11; row[1] = 22; row[2] = 33;
    garr[0] = &gvals[0];
    garr[1] = &gvals[1];
    printf("A=%d,%c\n", pp[1][0], sq[1][0]);
    printf("B=%d,%c\n", pp[0][1], sq[0][1]);
    printf("C=%d,%c\n", pp[1][1], sq[1][1]);
    printf("D=%d,%d\n", sum2(pp, 1, 1), sum2(pp, 1, 0));
    printf("E=%d,%c\n", arr[1][0], sa[1][0]);
    printf("F=%d,%d,%d\n", m[1][0], m[1][2], m[0][1]);
    printf("G=%d,%d\n", garr[1][0], rp[2]);
    return 0;
}
