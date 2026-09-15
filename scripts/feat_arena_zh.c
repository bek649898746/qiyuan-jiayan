// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 Arena 区域分配 (区域 分配/重置/释放) 接入测试 (2026-08-27):
 *   区域 分配(arena, 100);  →  arena_alloc(&arena, 100)
 *   区域 重置(arena);       →  arena_reset(&arena)
 *   区域 释放(arena);       →  arena_free(&arena)
 *   用户定义 arena_alloc/reset/free (bump 分配器, C 模拟已验语义)
 */

typedef struct { unsigned char buf[256]; int used; } Arena;

static void arena_init(Arena *a) { a->used = 0; }
static void *arena_alloc(Arena *a, int n) {
    if (a->used + n > 256) return 0;   /* 溢出 NULL */
    void *p = a->buf + a->used;
    a->used += n;
    return p;
}
static void arena_reset(Arena *a) { a->used = 0; }
static void arena_free(Arena *a) { a->used = 0; }

int main(void) {
    Arena arena;
    arena_init(&arena);

    /* ① 分配 + 写入 */
    int *p = 区域 分配(arena, 4 * sizeof(int));
    if (!p) return 1;
    p[0] = 10; p[1] = 20; p[2] = 30; p[3] = 40;
    if (p[3] != 40) return 2;

    /* ② 连续分配 */
    char *s = 区域 分配(arena, 16);
    if (!s) return 3;
    s[0] = 'a'; s[1] = 'b';
    if (s[1] != 'b') return 4;

    /* ③ 重置后复用 */
    区域 重置(arena);
    int *q = 区域 分配(arena, 8);
    if (!q) return 5;
    q[0] = 99;
    if (q[0] != 99) return 6;

    /* ④ 与已有特性组合 */
    范围 p[0] 于 0 到 100;
    夹取 q[0], 0, 50;   /* 99 → 50 */
    if (q[0] != 50) return 7;

    printf("PASS\n");
    return 0;
}
