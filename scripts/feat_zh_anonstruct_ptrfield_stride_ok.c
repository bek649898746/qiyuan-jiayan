// @EXPECTED out:ANONPTR 104 7 9
// 匿名结构体 指针字段 步长: `struct { FR *p; } h; h.p[1].b` 必须 按 sizeof(FR)=104 缩放。
// 修复前 (2421EAAB): 步长 8 ⟹ 写 不落 arr[1] ⟹ 输出 `ANONPTR 104 0 0` (静默错值);
// 修复后: 符肆/主挂 取 基类型尺寸 ⟹ `ANONPTR 104 7 9`。
#include <stdio.h>
typedef struct FR { char a[100]; int b; } FR;
static struct { FR *p; } h;
static FR arr[4];
int main(void) {
    h.p = arr;
    h.p[1].b = 7;
    h.p[2].b = 9;
    printf("ANONPTR %d %d %d\n", (int)sizeof(FR), arr[1].b, arr[2].b);
    return 0;
}
