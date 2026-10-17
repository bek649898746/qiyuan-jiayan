// @EXPECTED out: MAC180 42 42
/* 锚: 净缺口 #180「宏递归」的**正向面** —— 合法的宏链 / 宏内宏展开必须正常工作。
   （自引用宏被 blue paint 阻止、响亮报错，由负锚 `feat_zh_macro180_selfref_neg.c` 覆盖。）
   本锚证明两件事：
     ① 对象宏链：B → (A + 2) → (40 + 2) = 42
     ② 函数宏内调用另一函数宏：APPLY(41) → ADD(41) → ((41) + 1) = 42
   判据：编译 + 运行输出 `MAC180 42 42`（真实产物驱动，不看自述）。 */
#define A 40
#define B (A + 2)
#define ADD(x) ((x) + 1)
#define APPLY(x) ADD(x)
函数 整 主(空) {
    整 甲 = B;
    整 乙 = APPLY(41);
    格式化 "MAC180 %d %d\n", 甲, 乙;
    返 0;
}
