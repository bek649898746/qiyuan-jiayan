// @EXPECTED out:THR 1 11 21
// @EXPECTED exit:0
/* 第 447 站 族3 #45 线程模型完整 —— **真本体**锚（2026-10-03 虾米）
   ─────────────────────────────────────────────────────────────────────
   旧锚为何不算：`feat_thread_zh.c` **自带 `static int thread_create(...)` / `thread_join(...)`**
   （「确定性单线程模拟」) —— 按族3 扫描件的「**刀A**」判据：锚自造同名函数 ⟹ **遮蔽真本体**,
   只验脱糖连通性 ⟹ 降 🟡。
   本锚**不定义任何 thread_\*** —— 由**编译器直接发射真调用**（主源 L24803/L24820「线程真接入」）:
     `线程 t 跑 工作, 实参;` → `CreateThread(0,0,工作,实参,0,&t)`（L27423 导入）
     `等待 t;`                → `WaitForSingleObject(t, 无限); CloseHandle(t)`
   **确定性设计**（真线程有调度不确定性, 故避开竞争）: 每个线程只写**自己的槽** `结果[i]`,
   `等待` 三次后读 ⟹ 无数据竞争 ⟹ 值必然为 `1 / 11 / 21`（`i*10+1`）。
   若真本体缺失（只剩「用户模拟」）, 本锚要么编译不过, 要么读回全 0。 */
#include <stdio.h>

static int 结果[4];

static int 工作(void *arg) {
    int i = (int)(long long)arg;
    结果[i] = i * 10 + 1;
    return 0;
}

int main(void) {
    int t0, t1, t2;
    结果[0] = 0; 结果[1] = 0; 结果[2] = 0;
    线程 t0 跑 工作, (void*)(long long)0;
    线程 t1 跑 工作, (void*)(long long)1;
    线程 t2 跑 工作, (void*)(long long)2;
    等待 t0;
    等待 t1;
    等待 t2;
    printf("THR %d %d %d\n", 结果[0], 结果[1], 结果[2]);
    if (结果[0] == 1 && 结果[1] == 11 && 结果[2] == 21) return 0;
    return 1;
}
