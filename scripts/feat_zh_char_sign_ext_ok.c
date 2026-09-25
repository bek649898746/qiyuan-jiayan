/* feat_zh_char_sign_ext_ok.c — 裸 `char` 的符号扩展（x86-64 ABI: char = **signed**）
 *
 * R357（2026-09-25 · 窄整型截断族 —— 968 族表真缺陷之一）
 *
 * 病灶：裸 `char` 全线**零扩展**（局部变量 / 结构体字段 / 全局变量），
 *       而 `signed char` 与 `unsigned char` 都正确 ⟹ 三处「只认显式 signed」的同族判据：
 *         · 首词元路径（L10487-10489）不设 `深成`
 *         · 全局标量/数组（L19422/L19438）`若 (列利) …深成` 只认显式 signed
 *         · 结构体字段（L4716 体注册 / L17669 主解析器）`若 (单又) 副戌` 只认显式 signed
 *       而字段读判据（L25001）用 `副戌` 在 `movsx(0F BE)` / `movzx(0F B6)` 间二选一。
 *
 * 判据（gcc 复验：`char c3 = -1` 读回 -1，即 signed）：
 *   · 裸 char 四形态（局部 / 字段局部 / 字段全局 / 全局）== -55
 *   · signed char / unsigned char 不得回归（对照格）
 */
#include <stdio.h>

struct C { char f7; };
struct S { signed char f7; };
struct U { unsigned char f7; };
static struct C gc;
static struct S gs;
static struct U gu;
static char gv = -55;

int fails = 0;
#define CHK(n, g, w) do { \
    long long a = (long long)(g), b = (long long)(w); \
    if (a != b) { printf("FAIL %s: got %lld want %lld\n", n, a, b); fails++; } \
    else printf("ok %s\n", n); \
} while (0)

int main(void) {
    struct C c;
    char lc = -55;
    signed char ls = -55;
    unsigned char lu = 201;
    c.f7 = -55;
    gc.f7 = -55;
    gs.f7 = -55;
    gu.f7 = 201;

    CHK("char 局部变量", lc, -55);
    CHK("char 字段(局部实例)", c.f7, -55);
    CHK("char 字段(全局实例)", gc.f7, -55);
    CHK("char 全局变量", gv, -55);
    CHK("signed char 对照", ls, -55);
    CHK("unsigned char 对照", lu, 201);
    CHK("signed char 字段对照", gs.f7, -55);
    CHK("unsigned char 字段对照", gu.f7, 201);

    if (fails) { printf("RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT ALLOK\n");
    return 0;
}
