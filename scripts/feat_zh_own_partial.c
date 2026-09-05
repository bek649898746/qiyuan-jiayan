# 包含 <stdio.h>
/* 所有权 部分移动 分支精确 (2026-09-05 虾米 ring C): 若内 移入 目标, a.子 (非Copy字段部分移动) → else 读 a.子 兄弟隔离放行 (域六路径事件 撤销/汇合重投); @EXPECTED */
// @EXPECTED out:C BRANCH PARTIAL OK
构 内部 { 字 *p; 整 v; }
构 包 { 构 内部 子; 整 tag; }
构 包 建包(整 n) { 构 包 x; x.子.p = 0; x.子.v = n; x.tag = n; 返 x; }
整 读内(构 内部 x) { 返 x.v; }
函数 整 主(空) {
    构 包 a = 建包(1);
    构 内部 t; t.p = 0; t.v = 0;
    若 (a.tag > 100) { 移入 t, a.子; }   /* 运行时不执行; 编译期: 部分移动 a.子 登记 */
    否则 { 若 (读内(a.子) != 1) 返 1; }  /* else 读 a.子: 兄弟隔离 */
    格式化 "C BRANCH PARTIAL OK\n", 0;
    返 0;
}
