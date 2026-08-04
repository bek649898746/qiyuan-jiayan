/* probe_switch.c — switch/case/default 探测 */
int main() {
    int x = 2;
    int r = 0;
    switch (x) {
        case 1: r = 11; break;
        case 2: r = 22; break;
        case 3: r = 33; break;
        default: r = 99; break;
    }
    if (r != 22) return 1;
    /* default 路径 */
    x = 9;
    switch (x) {
        case 1: r = 11; break;
        default: r = 99; break;
    }
    if (r != 99) return 2;
    return 0;
}
