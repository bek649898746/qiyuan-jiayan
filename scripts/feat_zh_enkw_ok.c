// @EXPECTED exit:6
/* 放行锚 (2026-10-05 · 第 455 站·第一百零一轮): **英文 C 关键字 作 关键字 用** 必须 照常 工作。
   本轮 把 12 个 英文 语句关键字 (return/if/else/while/do/goto/for/break/continue/switch/case/default)
   并入 `语句关键字表` (声明位 撞名 硬收) —— 本锚 锁 **另 一面**: 它们 作 关键字 时 语句
   (while/for/do/switch/case/default/if/else/return/break) 行为 不变。
   若 扩表 波 及 语句解析 (例: 把 关键字 token 误判) ⟹ 本锚 红。 */
函数 整 主(空) {
    整 i = 0, s = 0;
    while (i < 3) { s = s + 1; i = i + 1; }
    for (i = 0; i < 2; i = i + 1) { s = s + 1; }
    do { s = s + 1; } while (0);
    switch (s) { case 6: s = 6; break; default: s = 99; break; }
    if (s != 6) return 99;
    return s;
}
