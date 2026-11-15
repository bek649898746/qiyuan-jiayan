/* 形态族：零实参调用（已修 → 回归守卫） */
int 取四十二(void) { return 42; }

int main(void) {
    return 取四十二();
}
