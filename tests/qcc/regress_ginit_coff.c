// @EXPECTED exit:0
// @EXPECTED out:0-1
// 回归: coff 模式 ginit 专用子程序 + 每函数守卫 (fix 2026-08-17)
// 原 ginit 只在最后一个函数内联发射 — 若该函数不被调用则全局初始化永不执行
// (git setup.o validate_ref_storage_format --help 不调 → startup_info=NULL 崩)
#include <stdio.h>

struct startup_info { int have_repository; };
static struct startup_info the_si;
struct startup_info *startup_info = &the_si;

static int validate_ref_storage_format(void) { return 0; }  /* 模拟最后函数 — 不被调用 */

int main(void) {
    int n = startup_info == 0;
    startup_info->have_repository = 1;
    int h = startup_info->have_repository;
    printf("%d-%d\n", n, h);
    return 0;
}
