// @EXPECTED out:SYST627 8 1 8 4 8
// @EXPECTED exit:0
// @EXPECTED silent:0
#包含 <stdio.h>
/* R627（2026-10-10 虾米 · 进IR前 T8 根治）**正锚** —— **系统类型名 = 内置 typedef**
   病灶: 甲言 **跳过 `<...>` 系统头** ⟹ `<stddef.h>`/`<stdint.h>/<sys/types.h>` 里的
     `size_t`/`int8_t`/`int64_t`/`int32_t`/`ptrdiff_t`/… **在本 TU 未登记** ⟹ 三面 同时 断:
       ① 声明 `extern size_t f(void);` ⟹ 名字 被当 类型 吞掉、真声明符 静默丢弃 ⟹ 调用点 **A1001**
          （真载体: lua `lobject.h` 的 `LUAI_FUNC size_t luaO_str2num(...)` ⟹ 6 个 lua 文件 编译 失败,
           堵死「编译真实项目」—— 见 `probes\r625_T8_返回类型位未知名\`）
       ② 转型 `(size_t)x`   ③ `大小(size_t)` —— 实测 lua `ldump.c` 报
          `A1001 大小: 'size_t' 未声明 —— 它 不是 变量/类型/结构体标签 …`
   修法: 在 解析() 复位后 把 该族 名 **登记为 内置 typedef**（按名 取 宽度: size_族 8 / int8 1 / int32 4 …）++
     `左系(名)` 让 词法器 认它 是 类型名 ⟹ **既有 通路**（前瞻的 型定判断 跳名 / 声明分支 / 转型 / 尺寸）
     自然 全 生效 —— 与 用户 自己 写 `typedef unsigned long size_t;` 时 **同行为**（d5 对照 早已 验证）。
   判据（五 格 · 全 为 区分性 值）: `大小(size_t)`=8 · `大小(int8_t)`=1 · `大小(int64_t)`=8 ·
     `大小(int32_t)`=4 · `大小(ptrdiff_t)`=8（对照格: 族内 三个 不同 宽度）。
   灵敏度: 父编（`0ECA3386`）上 本 锚 **编译失败**（`A1001 大小: 'size_t' 未声明`）⟹ **必红**。 */
函数 整 主(空) {
    格式化 "SYST627 %d %d %d %d %d\n", 大小(size_t), 大小(int8_t), 大小(int64_t), 大小(int32_t), 大小(ptrdiff_t);
    若 (大小(size_t) != 8) 返 1;
    若 (大小(int8_t) != 1) 返 2;
    若 (大小(int64_t) != 8) 返 3;
    若 (大小(int32_t) != 4) 返 4;
    若 (大小(ptrdiff_t) != 8) 返 5;
    返 0;
}
