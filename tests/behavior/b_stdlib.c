// Phase 2 标准库补齐回归: strrchr/memchr/memmove/atoi/atol/strtol/isspace/qsort/bsearch
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int icmp(const void *a, const void *b) {
    return *((int*)a) - *((int*)b);
}

int main() {
    // strrchr
    const char *s = "hello world hello";
    const char *p = strrchr(s, 'h');
    printf("strrchr=%d\n", (int)(p ? p - s : -1));
    // memchr
    char buf[16] = "abcdef";
    printf("memchr=%d\n", (int)(memchr(buf, 'd', 6) ? (char*)memchr(buf, 'd', 6) - buf : -1));
    // memmove 重叠
    char mv[8] = "abcdef";
    memmove(mv + 1, mv, 4);
    printf("memmove=%c%c%c%c%c\n", mv[0], mv[1], mv[2], mv[3], mv[4]);
    // atoi / atol / strtol
    printf("atoi=%d atol=%d\n", atoi("  -123"), (int)atol("456"));
    char *ep;
    long v = strtol("0x1A", &ep, 0);
    printf("strtol=0x%X ep='%s'\n", (unsigned)v, ep);
    // isspace
    printf("isspace=%d%d\n", isspace(' '), isspace('x'));
    // qsort
    int arr[7] = { 5, 1, 4, 2, 8, 0, 3 };
    qsort(arr, 7, sizeof(int), icmp);
    printf("qsort=%d%d%d%d%d%d%d\n", arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6]);
    // bsearch
    int key = 4;
    int *found = (int*)bsearch(&key, arr, 7, sizeof(int), icmp);
    printf("bsearch=%d\n", found ? *found : -1);
    return 0;
}
