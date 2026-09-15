/* A71-续2 (2026-09-15): 赋值基 成员 typo —— **负例** (期望 编译失败 A9001)。
   `(s = t).nope`: 底 是 节点_赋值 ⟹ 序七() 原 无 赋值 分支 → 类型 -1 → 静默 codegen;
   修: 序七() 赋值 分支 (值 = LHS 类型, 与 表达式是否无符号/是否长 同 判据)。 */
struct S { int a; long b; };

int main(void) {
    struct S s, t;
    s.a = 1; s.b = 2; t.a = 3; t.b = 4;
    return (s = t).nope;
}
