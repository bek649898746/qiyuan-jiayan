// @EXPECTED out:T1=3 T2=7 A=4 B=9
/* A8刀2 锚 (2026-09-24 启元 · 第 264 站): typedef 别名 作 括号声明符 的类型词.
   形态 `T (t1) = 3;` —— 词法阶段 T 是 词变量 (不是型词) ==> 原判据不归约 ==> 静默错值.
   旧编译器 (25D59B37) 实测: T1=541239372 T2=541239388 (垃圾); 修后 = 3 / 7. */
typedef int T;
typedef struct S { int a; int b; } S;
int main(void) {
    T (t1) = 3;
    T (t2) = 7;
    S (s1);
    s1.a = 4;
    s1.b = 9;
    printf("T1=%d T2=%d A=%d B=%d\n", t1, t2, s1.a, s1.b);
    return 0;
}