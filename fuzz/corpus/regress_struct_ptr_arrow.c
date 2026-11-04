// @EXPECTED exit:0
// @EXPECTED out:-100 114 114
#include <stdio.h>
/* 回归: 结构体指针初始化 + 箭头字段宽度 (fix 2026-08-07, fuzz 抓到)
   1) struct S *p = &s; 的 st_idx 也是结构体索引 → 误走大结构体拷贝分支 → p 存值非地址 → 解引用崩
      (sizeof(S) > 8 时触发; 指针守卫 var_pesz==0)
   2) 箭头 LL 字段 p->v = -100 存储缺符号扩展 → 4294967196; 读取无条件 32 位加载 → 高 32 位丢 */
struct S { char c; long long v; };
int main(void) {
    struct S s;
    struct S *p = &s;
    p->v = -100;          /* 箭头 LL 字段存储: 需 movsxd */
    p->c = 114;           /* 箭头 char 字段存储 */
    struct S *q = &s;     /* 大结构体指针初始化: 需存地址非结构体拷贝 */
    printf("%lld %d %d\n", q->v, q->c, p->c);
    return 0;
}
