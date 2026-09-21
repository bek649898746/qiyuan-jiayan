// @EXPECTED out:#R226 内核库扩展 PASS
#include <stdio.h>
#include <windows.h>   /* gcc 侧取真原型; 甲言跳过 <...> 系统头 ⟹ 走自身豁免表路径 */

/* ═══ 第 226 站（R226 · 2026-09-21）：内核库 IAT 扩容 17 → 64 槽 ═══
   背景: 调用生成的三路「名→槽」查询只覆盖 msvcrt 35 / 内核库 17 / math 24;
        表外任何 Win32 API 落静默桩「返 0」（第 225 站已给它装报警器）。
   本刀: 内核库槽 17 → 64（+47 个 git Windows 适配层实测高频 API）·
        名聚 0x900 → 0x1100（导入区让位）· 首厚一 0x1000 → 0x2000（.data raw size）·
        槽位编码重排（栈轮 / 终陆 / 帧退 / 左己[124] / 主亥 / 写表循环 / 段偏移 / 导入目录 RVA）。
   灵敏度: 上一代（E03D7572）上 本锚 **GetModuleHandleA 返回 0** ⟹ 必红。 */
int main(void) {
    /* ① 新登记 API（槽 19）—— 上一代恒返 0 */
    void *self = GetModuleHandleA(0);
    if (!self) { printf("FAIL 1: GetModuleHandleA=0\n"); return 1; }

    /* ② 旧槽回归位（槽 15，扩容后槽号不变） */
    void *k32 = LoadLibraryA("kernel32.dll");
    if (!k32) { printf("FAIL 2: LoadLibraryA=0\n"); return 2; }

    /* ③ 另两个新登记 API */
    unsigned int pid = (unsigned int)GetCurrentProcessId();
    if (!pid) { printf("FAIL 3: pid=0\n"); return 3; }
    void *heap = GetProcessHeap();
    if (!heap) { printf("FAIL 4: GetProcessHeap=0\n"); return 4; }

    /* ④ 错值往返（新登记，最常见的两类调用之一） */
    SetLastError(123);
    if (GetLastError() != 123) { printf("FAIL 5: GetLastError 丢值\n"); return 5; }

    printf("#R226 内核库扩展 PASS\n");
    return 0;
}
