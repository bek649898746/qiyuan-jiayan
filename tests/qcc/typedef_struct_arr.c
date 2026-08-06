// @EXPECTED exit:0
/* typedef struct 数组：只测元素访问求和（无指针运算）*/
typedef struct P { int a; int b; } P;

int main(void) {
    P arr[3];
    arr[0].a = 10; arr[0].b = 11;
    arr[1].a = 20; arr[1].b = 21;
    arr[2].a = 30; arr[2].b = 31;
    int sum = arr[0].a + arr[1].a + arr[2].a + arr[0].b + arr[1].b + arr[2].b;
    /* 10+20+30+11+21+31 = 123 */
    输出("sum=%d\n", sum);
    若 (sum != 123) { 输出("FAIL\n"); 返 1; }
    输出("PASS\n");
    返 0;
}
