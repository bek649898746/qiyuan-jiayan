// @EXPECTED out:#R227 指针转型长性 PASS
#include <stdio.h>
#include <windows.h>   /* gcc 侧取真原型; 甲言跳过 <...> 系统头 ⟹ 走自身豁免表路径 */

/* ═══ 第 227 站（R227 · 2026-09-21）：指针转型 ⟹ 64 位「长性」标记（R9-d）═══
   病史: `void* 字段 ← (void*)LoadLibraryA(...)` 得 0xFFFFFFFFC01A0000（高 32 位被符号扩展）
        ⟹ 存进结构体字段/数组后间接调用即崩 0xC0000005。赋给【局部变量】却正常
        （那条路径按变量声明宽度）—— 是「消费端在 RHS 码生成之前判宽」的判据缺标记。
   修法: ① `查返回名指()` 加「Win32 返回指针 API」静态名单 fallback ⟹ 调用节点带长标
        ② 转型表达式**出口**（所有包装节点之后）按 `深己 >= 1`（指针层数）设长标
   灵敏度: 上一代（8E214055）上 本锚 第 2/3 步错（field 值高 32 位全 1）⟹ 必红 */
struct Holder { void *h; void *(*fn)(void); };

int main(void) {
    void *k32 = LoadLibraryA("kernel32.dll");
    if (!k32) { printf("FAIL 1: k32=0\n"); return 1; }

    /* ① 调用结果直接赋给结构体字段（v2 补丁覆盖） */
    struct Holder ho;
    ho.h = LoadLibraryA("kernel32.dll");
    if (ho.h != k32) { printf("FAIL 2: 字段值 = %p\n", ho.h); return 2; }

    /* ② 带指针转型的调用赋给函数指针字段（原始病灶，v3 补丁覆盖） */
    ho.fn = (void *(*)(void))GetProcAddress((void *)k32, "GetCurrentProcessId");
    if (!ho.fn) { printf("FAIL 3: fn=0\n"); return 3; }

    /* ③ 真调用一次 —— 证明拿到的是可用指针而非「高 32 位坏掉的值」 */
    unsigned int pid = (unsigned int)ho.fn();
    if (!pid) { printf("FAIL 4: pid=0\n"); return 4; }

    printf("#R227 指针转型长性 PASS\n");
    return 0;
}
