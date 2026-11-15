/* 形态族：比较的无符号性 —— `unsigned a < 0x80` 必须发 `setb`（无符号），
   `int b < 0` 发 `setl`（有符号）。IR 原先恒按有符号发（`度细` 的第 2 参数恒 0）。 */
int main(void) {
    unsigned int a = 200;
    int b = -1;
    int r1 = a < 0x80 ? 1 : 0;
    int r2 = b < 0 ? 1 : 0;
    return r1 + r2;
}
