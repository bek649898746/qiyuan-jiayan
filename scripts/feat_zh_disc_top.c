# 包含 <stdio.h>
/* 判别 顶层定义+跨函数 (2026-09-06 D-1): 判别类型 顶层全局注册, helper 跨函数引用 — 吃狗粮 DOG-0 前置
   @EXPECT out:TD DTOPTD OK */
判别 借态 于 { 无借, 共享借, 独占借 };
static int 判值(int tag) {
    int r = 0;
    依据 r, tag 对照 借态_无借: 0, 借态_共享借: 1, 借态_独占借: 2;
    return r;
}
int main(void) {
    struct 借态 s;
    s.tag = 借态_共享借;
    if (判值(s.tag) != 1) { printf("FAIL 跨函数 %d\n", 判值(s.tag)); return 1; }
    s.tag = 借态_独占借;
    if (判值(s.tag) != 2) { printf("FAIL 跨函数2\n"); return 1; }
    if (借态_无借 != 0) { printf("FAIL 常量\n"); return 1; }
    printf("TD DTOPTD OK\n");
    return 0;
}
