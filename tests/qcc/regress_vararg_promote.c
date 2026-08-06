// @EXPECTED exit:0
// @EXPECTED out:5 2.500000 hi
#include <stdio.h>
/* 回归: 变参函数 (printf) 混合类型实参 — 各类型正确落位
   C 缺省实参提升只做 float->double; int 传给 %d, double 传给 %f,
   char* 传给 %s, 互不串位 (qcc 变参路径) */
int main(void) {
    printf("%d %f %s\n", 5, 2.5, "hi");
    printf("%d\n", 7);
    printf("%.2f %d\n", 3.14159, 42);
    printf("%s %d %s\n", "a", 1, "b");
    return 0;
}
