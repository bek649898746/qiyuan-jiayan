#包含 <stdio.h>
#包含 <windows.h>
// @EXPECTED out:SYSCALL252 1 1 1
// @EXPECTED exit:0
/* 族12 #252「系统调用接口（syscall）」**定案 (i) 证据锚**（R586 批C-AM · 2026-10-09）。
   定案命题: 「与操作系统交互」的**可观察面**在本语言中由 **导入表 (IAT)** 与 **动态解析**
     两条通道**完整承担**——**无裸 syscall 门**（无中断门、无调用号表；Windows 调用号随版本变，
     不稳定）。本锚把「同一程序三条 OS 交互通道拿到**同一**系统事实」钉死为可观察契约。

   三条通道（同一事实 = 本进程 pid）:
     ① **导入表直呼** —— `GetCurrentProcessId()`（`#包含 <windows.h>`；甲言跳过 `<>` 系统头，
        走自身豁免/核名表 ⟹ 真 `call [IAT]`）
     ② **甲言内建** `加载库` / `取函数` —— 编译期展开成 `load_library` / `get_proc_address`
        （即 LoadLibraryA / GetProcAddress 的真调用）
     ③ **普通调用** `LoadLibraryA` + `GetProcAddress` 取地址后间接调用
   判据（三格）: `p1 == p2 == p3` 且 `p1 != 0` ⟹ 输出 `SYSCALL252 1 1 1`。
   为何这是**真证据**而非同义反复: 三条走**不同**名字解析路径（IAT 槽 / 内建展开 / 运行期查表），
     任一条被退化成「假成功（返回 0 或桩地址）」⟹ 三者不可能相等且非零。
   ★ 对照（既有锚，共同构成面覆盖）: `feat_zh_dynload_plain_call_ok.c`（③ 路径防回归）·
     `feat_zh_mmap89.c`（内建/动态解析 承载真实 OS 操作：CreateFileMapping/MapViewOfFile）。 */
函数 整 主(空) {
    整 p1 = (整)GetCurrentProcessId();                                   /* ① 导入表 */
    整 (*f2)(空) = (整 (*)(空))取函数(加载库("kernel32.dll"), "GetCurrentProcessId"); /* ② 内建 */
    整 p2 = f2 ? f2() : 0;
    整 (*f3)(空) = (整 (*)(空))GetProcAddress(LoadLibraryA("kernel32.dll"), "GetCurrentProcessId"); /* ③ 普通调用 */
    整 p3 = f3 ? f3() : 0;
    整 同 = (p1 != 0 && p1 == p2 && p2 == p3) ? 1 : 0;
    格式化 "SYSCALL252 %d %d %d\n", 同, p1 != 0 ? 1 : 0, p2 != 0 ? 1 : 0;
    若 (同 != 1) 返 1;
    返 0;
}
