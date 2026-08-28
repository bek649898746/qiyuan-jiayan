#include <stdio.h>

/* 甲言 契约审判 接入测试 (2026-08-28 可吞清单 P0-7): 前置/后置 子句
   前置 (条件) = 入口检查 (编译器在函数体首注入 if(!前置){printf;exit;})
   后置 (条件) = 出口检查 (编译器在 epilogue 注入, 每个 return 都过; 返回 = 返回值占位)
   "AI 猜、证明器查" 的运行时审判形态 (负例见 feat_contract_neg_zh.c / _verify_contract.py) */

/* ① 前置+后置 均满足 → 正常返回 */
int div_safe(int a, int b) 前置 (b != 0) 后置 (返回 == a / b) {
    return a / b;
}

/* ② 多 return 路径: 后置在每个 return 都检查 */
int abs_val(int x) 后置 (返回 >= 0) {
    if (x < 0) return -x;   /* 早退路径也过检查 */
    return x;
}

/* ③ 前置 引用多个参数 */
int clamp(int v, int lo, int hi) 前置 (lo <= hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ④ 前置+后置 组合 (不变量: 结果在 [lo,hi] 内) */
int clip(int v, int lo, int hi) 前置 (lo <= hi) 后置 (返回 >= lo) 后置 (返回 <= hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

int main(void) {
    if (div_safe(10, 2) != 5) { printf("FAIL div\n"); return 1; }
    if (abs_val(-7) != 7) { printf("FAIL abs\n"); return 1; }
    if (clamp(5, 0, 10) != 5) { printf("FAIL clamp\n"); return 1; }
    if (clip(100, 0, 10) != 10) { printf("FAIL clip\n"); return 1; }
    if (clip(-5, 0, 10) != 0) { printf("FAIL clip2\n"); return 1; }
    printf("PASS\n");
    return 0;
}
