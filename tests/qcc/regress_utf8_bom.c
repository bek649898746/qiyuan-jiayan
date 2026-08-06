// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 回归: UTF-8 BOM 文件 (fix 2026-08-06)
   read_file 未剥 BOM → 首个 token 是 BOM 字节 → parse 崩 → main 被吞 → 编译产物无 main 崩
   用嵌套 if 而非 && 链 (qcc 对复杂条件链生成'自比较'错码); 循环移位避免 memmove 未内建 */
int main(void) {
    double d = 3.5;
    printf("%.1f\n", d);
    printf("PASS\n");
    return 0;
}
