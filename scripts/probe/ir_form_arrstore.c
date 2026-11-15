/* 形态族：数组元素写（循环内 + 变量索引 + 二元右值）—— `a[i] = i * i` */
int main(void) {
    int a[4];
    int i;
    for (i = 0; i < 4; i = i + 1) a[i] = i * i;
    return a[3];
}
