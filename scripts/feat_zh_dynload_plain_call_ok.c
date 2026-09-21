// @EXPECTED out:#R9 动态加载 PASS
#include <stdio.h>
#include <windows.h>   /* gcc 侧取 LoadLibraryA/GetProcAddress 真原型; 甲言跳过 <...> 系统头 ⟹ 走自身豁免表路径 */

/* ═══ R9 防回归锚（第 225 站 · 2026-09-21）═══
   挡的是「LoadLibraryA / GetProcAddress 用**普通调用**形式」这条路径。

   病史: 2026-09-17 审计记「LoadLibraryA 普通调用返回 0（新老编译器皆然）」——
        与 C18a「假成功」、C7「实参掩盖」同族:「prelude 有声明 + jyld 有白名单 ≠ 调用可用」。
   根因: 调用生成的三条「名→槽」查询里**缺内核库那条** ⟹ 调用落「未登记回退」桩 ⟹ 恒返 0。
   修复: 第 139 站（2026-09-20, 提交 68e270c）补 `核名[17]` + `核退()`，本锚自此挡回归。

   对照: 编译器**内建** `加载库`/`取函数`（编译期展开成 call_iat）是另一条独立路径，
        《scripts/feat_zh_mmap89.c》已覆盖；两者都要挡，故本锚不依赖 prelude。 */
int main(void) {
    /* ① 普通调用 LoadLibraryA（不是内建 加载库） */
    void *k32 = LoadLibraryA("kernel32.dll");
    if (!k32) { printf("FAIL 1: k32=0\n"); return 1; }

    /* ② 普通调用 GetProcAddress */
    void *(*p)(void) = (void *(*)(void))GetProcAddress(k32, "GetCurrentProcessId");
    if (!p) { printf("FAIL 2: fn=0\n"); return 2; }

    /* ③ 真调用一次 —— 证明拿到的不是「非 0 但不可用」的假句柄 */
    unsigned int pid = (unsigned int)(unsigned long long)p();
    if (!pid) { printf("FAIL 3: pid=0\n"); return 3; }

    printf("#R9 动态加载 PASS\n");
    return 0;
}
