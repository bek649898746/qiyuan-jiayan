/* 形态族：多实参调用（非"直接函数 + 单整数常量实参"） */
int 加(int a, int b) { return a + b; }

int main(void) {
    int x = 1;
    int y = 2;
    return 加(x, y);
}
