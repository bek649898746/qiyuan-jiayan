#include <stdio.h>
#include <string.h>

typedef int myint;
typedef myint myint2;          /* 嵌套 typedef */
typedef unsigned long long ull;
typedef char *string;
typedef int arr_t[3];
typedef int (*fp)(int, int);
typedef struct Point { int x, y; } Point;
typedef struct { int a; float b; } Anon;
typedef const char *const_str;

static int add(int a, int b) { return a + b; }

int main(void) {
    myint i = 42;
    myint2 j = i + 1;
    ull u = 18446744073709551615ULL;
    string s = "hello";
    arr_t arr = {1, 2, 3};
    fp f = add;
    Point p = {10, 20};
    Anon an = {5, 3};
    const_str cs = "world";

    printf("i=%d j=%d\n", i, j);
    printf("u=%llu\n", u);
    printf("s=%s len=%d\n", s, (int)strlen(s));
    printf("arr=%d%d%d\n", arr[0], arr[1], arr[2]);
    printf("f(2,3)=%d\n", f(2, 3));
    printf("p=%d,%d an=%d,%d\n", p.x, p.y, an.a, (int)an.b);
    printf("cs=%s\n", cs);
    printf("sizeof: myint=%d ull=%d string=%d arr_t=%d fp=%d Point=%d Anon=%d\n",
        (int)sizeof(myint), (int)sizeof(ull), (int)sizeof(string),
        (int)sizeof(arr_t), (int)sizeof(fp), (int)sizeof(Point), (int)sizeof(Anon));

    if (i != 42) return 1;
    if (j != 43) return 2;
    if (u != 18446744073709551615ULL) return 3;
    if (strcmp(s, "hello")) return 4;
    if (arr[2] != 3) return 5;
    if (f(2,3) != 5) return 6;
    if (p.y != 20) return 7;
    if (sizeof(myint) != 4) return 8;
    if (sizeof(ull) != 8) return 9;
    if (sizeof(string) != 8) return 10;
    if (sizeof(arr_t) != 12) return 11;
    if (sizeof(fp) != 8) return 12;
    if (sizeof(Point) != 8) return 13;
    printf("PASS\n");
    return 0;
}
