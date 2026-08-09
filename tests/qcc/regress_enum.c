// @EXPECTED exit:0
// 回归: enum 常量注册 + 本地 enum 变量 (fix 2026-08-09)
// 此前 enum 声明跳过常量体 {A,B,C}, 常量未注册 → 使用崩溃 0xC0000005
// 注: enum 负值 (= -2) 边角待修, 已记台账
enum Color { RED, GREEN, BLUE };
enum Suit { HEART = 1, DIAMOND = 5, CLUB };
int main(void) {
    if (RED != 0) return 1;
    if (GREEN != 1) return 2;
    if (BLUE != 2) return 3;
    if (HEART != 1) return 4;
    if (DIAMOND != 5) return 5;
    if (CLUB != 6) return 6;
    enum Color c = GREEN;
    if (c != 1) return 7;
    c = BLUE;
    if (c != 2) return 8;
    return 0;
}
