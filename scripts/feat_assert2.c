#include <stdio.h>
int main(void) {
    int x = 5;
    断言(x > 10);
    printf("不应到达\n");
    return 0;
}
