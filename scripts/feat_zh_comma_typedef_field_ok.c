// @EXPECTED out:CTF 1.5 3 2.5 7
/* 锚: 结构体「逗号声明」中 **typedef 构体类型**的非首字段 类型序 —— C113/R20 (第 108 站, 2026-09-18)
   症状: `typedef struct { Complex alpha, beta; } Qubit;` 之后 `q.beta.im` 报
        A9001「类型 'Qubit' 的字段 'beta' 不是 结构体/联合, 不能取成员」。
   根因: 文件级 `typedef struct {...} X;` 定义体的字段循环里, 字段类型序候选 `实肆` 的声明
        **写在循环体内** (`整 实肆 = -1;`) ⟹ 每轮重置 ⟹ 逗号续行 (C 语义: `beta` 继承基类型 `Complex`)
        拿不到类型 ⟹ 该字段「毛利」恒 -1。
        (对照: 顶层 `struct Q { Complex a, b; };` 走另一份块, 早已有 `虚挂` 继承 ⟹ 正常。
         D2 那份 (带标签的 typedef) 当年也为同一理由「把 域尺寸/行/实肆 **移出循环**」过 —— 本锚钉的是漏掉的那份。)
   修法: 把 `整 实肆 = -1;` **移出循环** (分号处已有重置) —— 与 D2 的既有修法同构, 零新机制。
   锚灵敏度 (实测): 旧编译器 (F19EBB9D) 上 **编译失败** (A9001) ⟹ 必红;
        新编译器 `CTF 1.5 3 2.5 7` rc=0; gcc 逐值同。
   覆盖形态: ① 逗号 + typedef 构体 ② 逗号 + 结构体数组 ③ 逗号 + 三字段 ④ 对照: 逗号 + 基本类型。
   ⚠️ 本锚保持最小 (不含「变量基的成员访问」) —— 那类访问会触发旧编译器的全表扫描兜底而使它变绿。 */
#include <stdio.h>

typedef struct { double re, im; } Complex;
typedef struct { int keys[4]; int vals[4]; } HashTable;

typedef struct { Complex alpha, beta; } Qubit;     /* ① 逗号 + typedef 构体 */
typedef struct { int n; HashTable t1, t2; } CH;    /* ② 逗号 + 结构体数组 */
typedef struct { Complex a1, a2, a3; } T3;         /* ③ 逗号 + 三字段 */
typedef struct { int x, y; } P;                    /* ④ 对照: 基本类型 (本来就对) */

int main(void) {
    int r = 0;
    Qubit q; q.beta.im = 1.5;
    CH c; c.t2.keys[1] = 3;
    T3 t; t.a3.im = 2.5;
    P p; p.y = 7;

    if (q.beta.im != 1.5) r |= 1;
    if (c.t2.keys[1] != 3) r |= 2;
    if (t.a3.im != 2.5) r |= 4;
    if (p.y != 7) r |= 8;

    printf("CTF %.1f %d %.1f %d\n", q.beta.im, c.t2.keys[1], t.a3.im, p.y);
    return r;
}
