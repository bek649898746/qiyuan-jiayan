/* 形态族：整比六种条件（已修 → 回归守卫；`<` 曾被编成 setle） */
int main(void) {
    int a = 3;
    int b = 5;
    int s = 0;
    if (a < b) s = s + 1;
    if (a <= b) s = s + 2;
    if (a > b) s = s + 4;
    if (a >= b) s = s + 8;
    if (a == b) s = s + 16;
    if (a != b) s = s + 32;
    return s;
}
