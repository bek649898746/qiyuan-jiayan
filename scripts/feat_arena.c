#include <stdio.h>
#include <stddef.h>

/* Arena 分配器 (Zig/Rust 模式): bump 分配 + 一次性整体回收
   竞技场生命周期: init → 分配 n 个对象 → reset 整体回收 → 再分配 */
typedef struct { char *buf; size_t size; size_t off; } Arena;

static void arena_init(Arena *a, char *buf, size_t size) {
    a->buf = buf;
    a->size = size;
    a->off = 0;
}
static void *arena_alloc(Arena *a, size_t n) {
    if (a->off + n > a->size) return NULL;   /* 溢出: 不越过竞技场边界 */
    void *p = (void *)(a->buf + a->off);
    a->off += n;
    return p;
}
static void arena_reset(Arena *a) { a->off = 0; }  /* 一次性回收整个竞技场 */

typedef struct { int x; int y; } Point;

int main(void) {
    char storage[256];
    Arena a;
    arena_init(&a, storage, sizeof(storage));

    /* 1. 分配标量 */
    int *p1 = (int *)arena_alloc(&a, sizeof(int));
    if (p1 == NULL) return 10;
    *p1 = 42;

    /* 2. 分配数组 */
    int *arr = (int *)arena_alloc(&a, 5 * sizeof(int));
    if (arr == NULL) return 11;
    for (int i = 0; i < 5; i++) arr[i] = i * i;

    /* 3. 分配结构体 */
    Point *pt = (Point *)arena_alloc(&a, sizeof(Point));
    if (pt == NULL) return 12;
    pt->x = 7;
    pt->y = 9;

    /* 4. 分配字符串块 */
    char *s = (char *)arena_alloc(&a, 6);
    if (s == NULL) return 13;
    s[0] = 'h'; s[1] = 'e'; s[2] = 'l'; s[3] = 'l'; s[4] = 'o'; s[5] = 0;

    printf("p1=%d arr3=%d pt=(%d,%d) s=%s off=%d\n", *p1, arr[3], pt->x, pt->y, s, (int)a.off);
    if (*p1 != 42) return 1;
    if (arr[3] != 9) return 2;
    if (pt->x != 7 || pt->y != 9) return 3;
    if (s[0] != 'h') return 4;
    if (a.off != sizeof(int) + 5 * sizeof(int) + sizeof(Point) + 6) return 5;

    /* 5. 一次性回收 (bump 复位), 重新分配 */
    arena_reset(&a);
    int *q = (int *)arena_alloc(&a, sizeof(int));
    if (q == NULL) return 14;
    *q = 100;
    if (*q != 100) return 6;
    if (a.off != sizeof(int)) return 7;

    /* 6. 溢出: 请求超过剩余 → NULL */
    char big[10];
    Arena b;
    arena_init(&b, big, 10);
    void *ok1 = arena_alloc(&b, 8);
    void *ok2 = arena_alloc(&b, 4);   /* 8+4 > 10 → NULL */
    if (ok1 == NULL || ok2 != NULL) return 8;

    printf("PASS\n");
    return 0;
}
