# 包含 <stdio.h>

/* #25 结构化并发: 结构化并发 { 体 } — 块内 线程 自动登记, 块退出自动 join 全部子线程
   (父作用域等待子任务完成; thread_create/thread_join 用户定义确定性模拟) */

static int 已跑 = 0;
static int join数 = 0;

static void thread_create(int *t, int (*fn)(void*), int arg) {
    (void)t; (void)fn;
    已跑 += arg;
}
static void thread_join(int *t) { (void)t; join数++; }

int main(void) {
    /* ① 块内 3 线程 → 块退出自动 join ×3 */
    已跑 = 0; join数 = 0;
    结构化并发 {
        线程 t1 跑 0, 10;
        线程 t2 跑 0, 20;
        线程 t3 跑 0, 30;
        if (已跑 != 60) return 1;   /* 体执行完: 3 线程已创建 */
    }
    if (join数 != 3) return 2;      /* 块退出自动 join ×3 */

    /* ② 嵌套结构化并发: 内层 2 + 外层 1 → join 3+1 */
    已跑 = 0; join数 = 0;
    结构化并发 {
        线程 a1 跑 0, 1;
        结构化并发 {
            线程 b1 跑 0, 2;
            线程 b2 跑 0, 3;
        }
        if (join数 != 2) return 3;  /* 内层退出 join ×2 */
        线程 a2 跑 0, 4;
    }
    if (join数 != 4) return 4;      /* 外层退出 join a1+a2 = 2 (内层 2 + 外层 2 = 4) */

    /* ③ 块外 线程 不登记 (普通) */
    join数 = 0;
    {
        线程 x 跑 0, 5;
    }
    if (join数 != 0) return 5;      /* 普通块不 join */

    printf("#25 结构化并发 PASS\n");
    return 0;
}
