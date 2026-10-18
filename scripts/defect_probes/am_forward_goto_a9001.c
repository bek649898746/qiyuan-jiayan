/* 真缺陷探针 · am.c 前向 goto 标号（2026-10-17 挂账）
   真实载体: git `builtin/am.c:1867` `goto commit;` → 标号 `commit:` 在 `:1901`（同函数后置）
   期望（修好后）: rc=0；当前: rc=1 A9001「跳转目标标号 'done' 在本函数内未定义」。 */
int probe_forward_goto(int x)
{
    if (x)
        goto done;
    return 1;
done:
    return 0;
}
