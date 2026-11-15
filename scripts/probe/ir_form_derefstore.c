/* 形态族：解引用写（`*p = v`，标量宽度 1/2/4/8 —— 窄读符号性回归守卫） */
int main(void) {
    int a = 0;
    int b = 0;
    char c = 0;
    short s = 0;
    long long l = 0;
    int *p = &a;
    char *pc = &c;
    short *ps = &s;
    long long *pl = &l;
    *p = 11;
    *pc = 12;
    *ps = 13;
    *pl = 14;
    b = a + (int)c + (int)s + (int)l;
    return b;
}
