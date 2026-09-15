// @EXPECTED out:PASS
#include <stdio.h>
#include <stdlib.h>

/* 所有权/移动: C 手动内存管理 + 指针移动语义 */
typedef struct {
    int *data;
    int n;
} Buffer;

static Buffer make_buffer(int n) {
    Buffer b;
    b.n = n;
    b.data = (int *)malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) b.data[i] = i * i;
    return b;
}

/* 移动: 转移所有权 (源置空) */
static Buffer move_buffer(Buffer *src) {
    Buffer b = *src;
    src->data = NULL;
    src->n = 0;
    return b;
}

static void free_buffer(Buffer *b) {
    free(b->data);
    b->data = NULL;
    b->n = 0;
}

int main(void) {
    Buffer a = make_buffer(5);
    if (!a.data || a.n != 5) return 1;
    if (a.data[4] != 16) return 2;

    Buffer b = move_buffer(&a);       /* 移动: b 接管, a 失效 */
    if (a.data != NULL || a.n != 0) return 3;
    if (b.data[2] != 4) return 4;

    int total = 0;
    for (int i = 0; i < b.n; i++) total += b.data[i];
    if (total != 30) return 5;

    free_buffer(&b);                  /* 释放 */
    if (b.data != NULL) return 6;
    printf("PASS\n");
    return 0;
}
