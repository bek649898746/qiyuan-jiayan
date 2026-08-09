// @EXPECTED exit:0
// 回归: 全局 enum 变量声明 (fix 2026-08-09)
// 根因: 顶层 enum 处理器假设必有常量体 {…}, 遇 enum Color g_color; 直接 continue
//       吞掉变量声明 → g_color 未注册 → 运行时写地址 0 → 0xC0000005
enum Color { RED = 1, GREEN = 2 };
enum Color g_color = GREEN;   /* 带初始化器 */
enum Color g2;                /* 无初始化器 */

int main(void) {
    if (g_color != GREEN) return 1;
    if (sizeof(g_color) != 4) return 2;
    g2 = RED;
    if (g2 != RED) return 3;
    g_color = RED;
    if (g_color != RED) return 4;
    return 0;
}
