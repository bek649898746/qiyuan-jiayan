// @EXPECTED err:A9001
/* A71-续 (2026-09-15): 解引用基 成员 typo —— **负例** (期望 编译失败 A9001)。
   `(*p).nope`: 底 是 节点_解引用 ⟹ 原 检查 缺 此 家族 → 静默;
   修: 序七() 的 节点_解引用 分支 已 下钻 到 操作数 类型 (p 是 结构体 S *, 取 出 S 的 字段表)。 */
struct S { int a; };

int main(void) {
    struct S s;
    struct S *p = &s;
    p->a = 1;
    return (*p).nope;
}
