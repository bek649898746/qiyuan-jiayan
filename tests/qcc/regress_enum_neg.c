// @EXPECTED exit:0
// 回归: enum 负值常量 + typedef enum 负值 (fix 2026-08-09)
// 根因1: 顶层/typedef enum 处理器不消费 MK(-) → RED = -2 卡死死循环
// 根因2: e_lookup 用 -1 作"未找到"哨兵, 与负值常量冲突 → RED=-2 被当普通变量=0
enum Color { RED = -2, GREEN = 5, BLUE = 1 };
typedef enum { NEG = -3, ZERO = 0, POS = 7 } Level;

int main(void) {
    if (RED != -2) return 1;
    if (GREEN != 5) return 2;
    if (BLUE != 1) return 3;
    if (RED < 0) { } else return 4;
    if (NEG != -3) return 5;
    if (ZERO != 0) return 6;
    if (POS != 7) return 7;
    enum Color c = RED;
    if (c != -2) return 8;
    Level l = NEG;
    if (l != -3) return 9;
    return 0;
}
