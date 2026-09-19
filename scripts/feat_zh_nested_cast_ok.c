// @EXPECTED out:NC 3.00 3.00 3.00 3.00 6.00 7.00
/* 锚: **嵌套转型** `(双精度)(整数)v` —— C127b (2026-09-19 第 122 站)
   症状: `double v = 3.5; double r = (double)(int)v;` ⟹ 旧编译器 **`3.50`** (gcc `3.00`) ——
     **内层 `(int)` 被吞**, 结果等于原 v 值 (静默错值)。
     ⚠️ 单层转型全对 (`(int)v` ✓ / `(double)i` ✓); ⚠️ `(double)(int)v` 作**实参**时旧编译器给 `0.00`。
   根因: `(双精度)X` 是**无操作转型** —— 不建新节点, 直接在 X 的节点上置 `节浮标` (主源 L8638);
     而 `(整数) 于 双精度表达式` 会建**节点19 截断节点** (L8599, 语义 = cvttsd2si, 自带 `节浮标[央]=0`)。
     外层 再 置 `节浮标[节点19] = 1` ⟹ `码生成浮` 走到 `若 (节浮标[数]) { 码生成(数); 返; }`
     ⟹ **只发截断不升级** ⟹ 消费端取 xmm0 (= 更早的浮点残值) ⟹ 得 3.5。
   修法: **保留** 外层浮标 (判定不丢信息), 在 `码生成浮` 里为**转型包装节点** (19 截断 / 27 窄截断)
     加分支 `若 (暂 == 19 || 暂 == 27) { 码生成(数); 旧挂(); 返; }` —— 先算整数再 `cvtsi2sd` 升成 xmm0
     (与已有的「整数立即数 → 转换」分支同形)。
     ⚠️ 首版 (C127) 曾用「守卫: 不给 19/27 设浮标」—— 修好了赋值路径, 但**实参路径丢失浮点信息**
     ⟹ `printf("%.2f", (double)(int)v)` 给 `0.00`。改用本刀后两条路径同时正确。
   锚灵敏度 (实测): 旧编译器 (qcc_zh_seed.exe = D1913329) 上 `NC 3.50 3.50 3.50 …` 必红;
     新编译器 `NC 3.00 3.00 3.00 3.00 6.00 7.00` rc=0; gcc 逐值同。
   覆盖形态: ① `(double)(int)v` ② 加括号 ③ `(double)(long long)w` ④ 三层嵌套 ⑤ 参与运算 ⑥ 对照单层。
   ⚠️ **不含** `(double)(char)v` / `(double)(short)v` / `(double)(unsigned)v` —— 那是**同族但另一根因**
     (节点27 的 codegen 假定输入已在 eax, 缺 `cvttsd2si`; `(unsigned)` 则根本不建截断节点), 已挂账。 */
#include <stdio.h>

int main(void) {
    double v = 3.5;
    double w = 3.9;
    int i = 7;

    double a = (double)(int)v;              /* ① 单层嵌套 */
    double b = (double)((int)v);            /* ② 加括号 */
    double c = (double)(long long)w;        /* ③ 64 位内层 */
    double d = (double)(int)(float)v;       /* ④ 三层 */
    double e = (double)(int)v + (double)(int)v;  /* ⑤ 参与运算 */
    double f = (double)i;                   /* ⑥ 对照: 单层转型 */

    printf("NC %.2f %.2f %.2f %.2f %.2f %.2f\n", a, b, c, d, e, f);
    return 0;
}
