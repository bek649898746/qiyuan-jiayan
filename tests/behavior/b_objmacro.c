// 对象宏/别名宏展开回归 (fix 2026-08-13: Git hash-ll.h TAB 别名)
#define A B
#define B C
#define C 42
#define git_SHA_CTX\tplatform_SHA_CTX
typedef unsigned int platform_SHA_CTX;
#define GIT_MAX 32
#define NEGALIAS -7
#include <stdio.h>
int main() {
    git_SHA_CTX c = A;
    int arr[GIT_MAX];
    arr[0] = NEGALIAS;
    printf("c=%u arr=%d\n", c, arr[0]);
    if (c != 42) return 1;
    if (arr[0] != -7) return 2;
    return 0;
}
