// @EXPECTED compile_fail
/* R1-B 锤B (2026-09-04) #98 负例1: 变体载荷绑定 数不匹配 → 编译期拦截 */
int main(void) {
    判别 Shape 于 { 圆(双 半径), 点() };
    struct Shape c;
    c.tag = Shape_圆; c.半径 = 2.0;
    双 area;
    依据 area, c.tag 穷尽 Shape 对照 圆(半径, 多余): 1.0, 点(): 0;
    return 0;
}
