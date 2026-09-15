// @EXPECTED out:ANON COMMA INHERIT PASS
// @EXPECT out:ANON COMMA INHERIT PASS
# 包含 <stdio.h>
/* 2026-09-10 修复锚: 匿名构体 逗号字段类型继承 (C 语义 `双 一, 二` → 二 也是 双)
   修复前: 匿名构体字段循环 每字段重置类型状态 → 二 落 整 4B → `二 = 2.5` 报 A9002 (C2)
   (= feat_zh_generic_typedef46 泛型 typedef 展开成 匿名构 后暴露 的同一根因) */
函数 整 主(空) {
    构 { 双 一, 二 } v;      /* 1 个匿名构: 二 继承 双 (8B) */
    v.一 = 1.5; v.二 = 2.5;
    若 (v.一 != 1.5 || v.二 != 2.5) { 格式化 "FAIL comma inherit\n", 0; 返 1; }
    格式化 "ANON COMMA INHERIT PASS\n", 0;
    返 0;
}
