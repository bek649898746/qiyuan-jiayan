// @EXPECTED out:R798F FNTYPE ALIAS OK
// @EXPECTED exit:0
# 包含 <stdio.h>
/* ★ R798f/g (2026-10-13 虾米 · git 全量打磨 · A9010 18 文件族): **函数类型别名 的 指针 是 函数指针**。

 * 语义 (C 6.7.5.3 / 6.5.2.2): `类型定义 空 F1(整, 整);` 的 F1 是 **函数类型**;
 *   `F1 *p` 里 那 一颗 `*` **正是 函数指针本身** ⟹ p 求值后 层级 = **0** (标量函数指针, 可调用)。
 *   对照: `类型定义 空 (*F2)(整, 整);` 的 F2 **本身 已 是 指针** ⟹ `F2 *pp` 才 是
 *   「指向函数指针的指针」(层级 1, 不可直接调用 —— 须 `(*pp)(…)`)。
 * 病灶 (修前): 二者 在 型定表 里 `函层` 同 为 0 (型定加函数指 不区分 类型/指针 别名) ⟹
 *   声明 `F1 *p` 时 那 颗 `*` 被 多 记 一层 ⟹ **假 A9010**:
 *   「被调者 p 是 变量, 但 求值后 层级 = 1 (>0, 指向函数指针的指针), 不可调用」。
 * 真载体 (git 2.45.2): `builtin/commit-graph.c` 的 `parse_opt_subcommand_fn *fn`
 *   (parse-options.h: `typedef int parse_opt_subcommand_fn(int, const char **, const char *);`)
 *   · `trace2/tr2_ctr.c` 的 `tr2_tgt_evt_counter_t *fn_apply`
 *   (trace2/tr2_tgt.h: `typedef void(tr2_tgt_evt_counter_t)(const struct …, …);` — 括号化 声明符 形态)
 *   · 同族 共 18 个 git 文件 (builtin_bisect/bundle/gc/hook/reflog/remote/stash/worktree/…)。
 * 修法: 型定表 增 `是否函类型` 位 (型定加函数指 第 4 参) + 声明侧 五 条 路径 (局部 标量/局部 数组/
 *   全局/形参/局部 指针分支) 统一 经 `型定函层净` 语义 (函类型别名 减 1) 记 A22 层级。
 * 负面对照 (既有 `feat_zh_*` A9010 负锚 全绿): 层级 2 的 `整 (**g)(整)` 裸调 / 数据指针 `整 *p` 调用
 *   / `&函数指针变量` —— 仍拒 (本锚 不 动 那些判据, 只 修 层级 记账)。
 * 探针 (可复算): `qiyuan-jiayan\_audit_inbox\20261013_虾米_12套起点表_闭环脚手架\evidence\probes\r798f_a9010_fntype\`
 *   (6 个 .c + run.log: 修前 3 红 / 修后 6 全绿)。 */
类型定义 整 甲型(整);
类型定义 空 己型(整, 整);

静 整 加一(整 甲) { 返 甲 + 1; }
静 整 计 = 0;
静 空 记二(整 甲, 整 乙) { 计 = 甲 + 乙; }

函数 整 主(空) {
    甲型 *f = 加一;              /* 函数**类型**别名 + 一颗 `*` = 函数指针 (层级 0) */
    整 甲 = f(41);
    若 (甲 != 42) { 格式化 "FAIL 甲型 %d\n", 甲; 返 1; }

    己型 *g = 记二;              /* 无返回值 函数类型别名 同款 */
    g(3, 4);
    若 (计 != 7) { 格式化 "FAIL 己型 %d\n", 计; 返 2; }

    格式化 "R798F FNTYPE ALIAS OK\n", 0;
    返 0;
}
