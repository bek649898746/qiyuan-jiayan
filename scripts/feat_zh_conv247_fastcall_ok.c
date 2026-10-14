// @EXPECTED exit:0
// @EXPECTED out:CONV247 乙=44
#包含 <stdio.h>
/* 族2 #247 调用约定 —— **定案反转**（2026-10-15 虾米 · C3 · 依据 = 真实语料）:
   原定案（2026-10-08 · 提交 99a0a54f）声明 `__stdcall` / `__fastcall` ⟹ **响亮拒绝 A6096**，
   并留下两个 `_neg` 锚。**本轮用语料事实推翻该定案**:
     · x64 **只有一种**调用约定 ⟹ 前缀 语义上可安全忽略（MSVC 自身亦忽略）;
     · 真实语料大量携带: zlib `zconf.h` `__stdcall`×9 + `WINAPI`（**zlib 是本项目 7 套基线之一**）·
       sqlite `sqlite3.c` `__cdecl`×97 + `WINAPI` + `CALLBACK` · libpng `pngconf.h` `__cdecl` ·
       curl / redis `CALLBACK` ⟹ 若改「响亮拒绝」, zlib / sqlite **当场大面积红**;
     · ⟹ **拒绝 = 用一条纸面定案打断真实语料**。
   裁定: **收下（词法消掉）**; 本锚把「接受 + 语义不变」钉死。
   ⚠️ 中文词形 `调用约定` 仍 A6096（我方自造拼法，应响亮提示）—— 见 `feat_zh_conv247_cdecl_neg.c`。
   ⚠️ 同刀记录: A6096 判据（主源 L20803）原列的 `__stdcall` / `__fastcall` / `__cdecl` 三个拼法
     **在词法期已被消掉** ⟹ 该判据对它们**永不可达**（死分支）, 同刀清理。 */
__fastcall 整 乙(整 x) { 返 x + 2; }
函数 整 主(空) {
    格式化打印("CONV247 乙=44\n", 乙(42));
    返 0;
}
