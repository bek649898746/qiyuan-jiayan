// @EXPECTED exit:0
// @EXPECTED out:65 A 89 Y
#include <stdio.h>
/* 回归: 字符串字面量下标 "abc"[i] (fix 2026-08-06)
   原 bug: STR primary 无 suffix chain → 实参 `f("A"[0])` 解析卡死;
   赋值场景留下未消费的 `[` → 走指针参数分支取垃圾地址 (4205178 而非 65) */
int f(int x) { return x; }

int main(void) {
    int a = "A"[0];
    if (a != 65) return 1;
    if (f("A"[0]) != 65) return 2;      /* 实参场景 (原挂死) */
    if ("ABC"[2] != 'C') return 3;      /* 索引正确 */
    if ("A"[0] != 65) return 4;
    printf("%d %c %d %c\n", "A"[0], "A"[0], "XYZ"[1], "XYZ"[1]);
    return 0;
}
