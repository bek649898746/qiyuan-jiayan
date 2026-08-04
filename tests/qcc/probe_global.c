/* probe_global.c — 全局变量声明(标量) 探测 */
int g_count = 7;
int g_zero;

int main() {
    if (g_count != 7) return 1;
    if (g_zero != 0) return 2;
    g_count = g_count + 1;
    if (g_count != 8) return 3;
    return 0;
}
