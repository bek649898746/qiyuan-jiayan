// @EXPECTED out:PASS
#include <stdio.h>

/* 标签名 与 类型名/struct tag 同名 —— ORD-20261104-106
 *
 * 病灶（修前）：块() 的「类型名 → 声明」快路径 先于 标签判定 ⟹ `标识 :` 里的 标识
 *   若撞了 struct tag / typedef 名，就被当**类型**消费掉 ⟹ 标签 从未登记 ⟹
 *   同函数内 `goto 同名` 在收尾检查 报 **假** A9001（「目标标签未定义」）。
 *   C11 6.2.3：label 是**独立名字空间**（类 1），与 普通标识符/类型名（类 4）本就不冲突。
 *   影响面（审计精确扫描）≈561 文件：openssl `end:`×240、FFmpeg `end:`×152、git `commit:` …
 *
 * 修法（单点）：块内语句分派口 在**任何声明/类型判定之前**，把 `词变量 + 冒号`
 *   直接交给 语句() 的标签分支（登记 标定名 + 处理 `标签: 循环/遍/做` 包装）。
 *
 * 判据（施工单 §五 1/2/6 的正向部分；4/5 由既有负锚 gotolabel_neg / gotokw_neg 守）：
 *   ① struct tag 同名标签 + goto ② typedef 名同名标签 + goto ③ enum tag 同名（对照组）
 *   ④ 局部变量同名标签（名字空间独立） ⑤ 无冲突 ⑥ 标签后跟循环（包装路径不回归）
 * 灵敏度：修复前 A9001（'commit' 未定义）/ A1004（'tdef_name' 被当类型）；
 *   修复后 PASS（且值与 gcc 一致）。
 */

struct commit { int x; };            /* ① struct tag */
typedef int tdef_name;               /* ② typedef 名 */
enum e_tag { E_VAL = 41 };           /* ③ enum tag（修前修后都应绿） */

static int f_struct_tag(int c) {
    struct commit v;
    v.x = 5;
    if (c) goto commit;              /* 前向 goto，目标与 struct tag 同名 */
    (void)c;
commit:                              /* ← 修前被当类型吞掉 */
    return v.x;
}

static int f_typedef(int c) {
    tdef_name n = 7;
    if (c) goto tdef_name;           /* 目标与 typedef 名同名 */
    (void)c;
tdef_name:                           /* ← 修前被当「类型: 声明」→ A1004 */
    return n;
}

static int f_enum_tag(int c) {
    if (c) goto e_tag;
    (void)c;
e_tag:
    return E_VAL;
}

static int f_local_var(int c) {
    int same = 9;
    (void)same;
    if (c) goto same;                /* 目标与局部变量同名 */
same:
    return 3;
}

static int f_none(int c) {
    if (c) goto plain;
    (void)c;
plain:
    return 4;
}

static int f_label_loop(int c) {
    int n = 0;
    if (!c) goto wrap;
wrap: 循环 (n < 2) { n++; }          /* 标签后跟循环：包装路径 */
    return n;
}

int main(void) {
    int bad = 0;
    if (f_struct_tag(1) != 5) { printf("FAIL struct_tag %d\n", f_struct_tag(1)); bad++; }
    if (f_typedef(1) != 7) { printf("FAIL typedef %d\n", f_typedef(1)); bad++; }
    if (f_enum_tag(1) != 41) { printf("FAIL enum_tag %d\n", f_enum_tag(1)); bad++; }
    if (f_local_var(1) != 3) { printf("FAIL local_var %d\n", f_local_var(1)); bad++; }
    if (f_none(1) != 4) { printf("FAIL none %d\n", f_none(1)); bad++; }
    if (f_label_loop(0) != 2) { printf("FAIL label_loop %d\n", f_label_loop(0)); bad++; }

    if (bad) { printf("FAIL total=%d\n", bad); return 1; }
    printf("PASS\n");
    return 0;
}
