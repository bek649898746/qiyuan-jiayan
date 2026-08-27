#include <stdio.h>

/* 甲言互斥锁临界区 (锁住) 接入测试: 锁住 m { body } → mutex_lock(&m); body; mutex_unlock(&m);
   用户定义 mutex_lock/mutex_unlock (约定名); v1 块尾解锁 (提前 return 不解锁留后续) */

static int m = 0;
static void mutex_lock(int *p) { (*p)++; }
static void mutex_unlock(int *p) { (*p)--; }

int main(void) {
    /* 1. 基本临界区: 进入锁, 退出解锁 */
    m = 0;
    锁住 m {
        if (m != 1) return 1;   /* 已加锁 */
    }
    if (m != 0) return 2;       /* 已解锁 */

    /* 2. 临界区操作: 保护计数器 */
    {
        int counter = 0;
        int lock = 0;
        锁住 lock {
            counter++;
            if (lock != 1) return 3;
        }
        if (lock != 0) return 4;
        if (counter != 1) return 5;
    }

    /* 3. 嵌套锁住 (不同锁) */
    {
        int a = 0, b = 0;
        锁住 a {
            锁住 b {
                if (a != 1 || b != 1) return 6;
            }
            if (b != 0) return 7;   /* 内层已解锁 */
        }
        if (a != 0) return 8;
    }

    /* 4. 循环内锁住 */
    {
        int lock = 0;
        int sum = 0;
        for (int i = 0; i < 3; i++) {
            锁住 lock {
                sum += i;
            }
        }
        if (sum != 3) return 9;
        if (lock != 0) return 10;
    }

    /* 5. 锁住 + 分支 */
    {
        int lock = 0;
        int v = 5;
        锁住 lock {
            if (v > 3) {
                v = 100;
            }
        }
        if (v != 100) return 11;
        if (lock != 0) return 12;
    }

    printf("l1=%d l2=%d l3=%d sum=%d\n", 1, 0, 0, 3);
    printf("PASS\n");
    return 0;
}
