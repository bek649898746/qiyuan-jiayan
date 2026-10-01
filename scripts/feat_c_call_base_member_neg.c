// @EXPECTED err:A9001
/* A71-续2 (2026-09-15): 调用基 成员 typo —— **负例** (期望 编译失败 A9001)。
   `mk(3).nope`: 底 是 节点_调用 ⟹ 序七() 原 无 调用 分支 → 类型 -1 → 静默 codegen;
   修: 序七() 调用 分支 (调用构返 = 按值返回构体类型序)。 */
struct S { int a; long b; };

struct S mk(int v) { struct S r; r.a = v; r.b = v * 10; return r; }

int main(void) {
    return mk(3).nope;
}
