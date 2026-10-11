#include <stdio.h>
// @EXPECTED out:IRLOT2 5 12 3 7
// @EXPECTED exit:0
/* R714 (2026-10-11 虾米) · IR 层「全面接入」批 N1/N2 综合锚 —— 同锚驱动 本批四个新落位位 + 既有五位:
   ① 达集 points-to  ← `int * restrict 丙 = &甲;` (restrict 基对象登记 + 变量登记后回填源变)
   ② 性录 IR 元数据  ← `__attribute__((aligned(8)))` (属性登记 + aligned 参值回填)
   ③ 跃表 决策表     ← `判别 Shape 于 {…}` (判别登记 → 判别值域 [0, 变体数-1])
   ④ 型达 类型表达式 ← 类型别名 登记 (型定加 复合类型)
   ⑤ 既有: 细化(#123)→左戊一/半退三 · 单继承→度散 · 循环/条件→控式 · 窄转型→段双
   判据: **行为不变**(落位 = 旁写侧表, 预埋零侵入); 落位真值 由 QCC_DUMP=ir 只读视图 留证。 */
struct __attribute__((aligned(8))) 物构 { int 值; };
判别 Shape 于 { 圆(半径), 方(边长), 点() };
typedef int * 指针别名;
struct 基 { int 甲; };
struct 子 : 基 { int 乙; };
int main(void) {
    细化 整(1, 10) 细 = 5;
    int 甲 = 7;
    int * restrict 丙 = &甲;
    int 和 = 0;
    遍 (int 序 = 0; 序 < 3; 序++) { 若 (序 == 1) 和 = 和 + 10; 否 和 = 和 + 1; }
    字 窄 = (字)300;
    struct 子 物; 物.甲 = 1; 物.乙 = 2;
    printf("IRLOT2 %d %d %d %d\n", 细, 和, 物.甲 + 物.乙, (int)(*丙));
    return 0;
}
