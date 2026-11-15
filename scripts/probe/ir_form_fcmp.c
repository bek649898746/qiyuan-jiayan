/* 形态族：浮点比较（`d > e` / `d < e` / `d == e` / `d != e`） */
int main(void) {
    double d = 3.7;
    double e = 2.5;
    int n = 0;
    if (d > e) n = n + 1;
    if (d < e) n = n + 2;
    if (d >= e) n = n + 4;
    if (d <= e) n = n + 8;
    if (d == e) n = n + 16;
    if (d != e) n = n + 32;
    return n;
}
