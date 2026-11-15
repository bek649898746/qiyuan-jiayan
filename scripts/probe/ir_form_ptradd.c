/* 形态族：指针加（`p + n` 带元素缩放） */
int main(void) {
    int a[3];
    int *p;
    a[0] = 1;
    a[1] = 2;
    a[2] = 3;
    p = a;
    return *(p + 1);
}
