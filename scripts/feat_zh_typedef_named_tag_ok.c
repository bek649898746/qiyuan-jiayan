// @EXPECTED out:NAMED TAG 8 2 3 8 8 4
#include <stdio.h>
/* 零格锚 (启元 复盘 序8 矩阵 报出, 2026-09-13): **C 具名标签 typedef** 与 **别名 反射**
   —— `decl_form_matrix.py` 实测 三 个 形态 零格 (有 解析路径, 无 锚):
     ① `typedef struct Tag {...} Alias;`  (具名标签+体+别名 — 型定构标 路径)
     ② `typedef union  Tag {...} Alias;`  (同上 + 联合语义)
     ③ `sizeof(((Alias *)0)->字段)`        (反射 走 **别名** 而非 struct/构 关键字)
   本锚 一次 覆盖 三 格; 值 全部 与 gcc 对照 一致 (gcc 基准: 8 2 3 / 8 8 / 4)。
   语义 硬点: 具名 union 必须 是 联合 (sizeof 取最大 = 8), 不是 结构体求和 (16)。 */
typedef struct TagS { int a; char b[2]; } AliasS;
typedef union TagU { int i; double d; } AliasU;
函数 整 主(空) {
    AliasS s;
    s.a = 1;
    s.b[0] = 2;
    AliasU u;
    u.i = 0;
    若 (大小(AliasS) != 8) { 格式化 "FAIL ① 尺寸 %d\n", 大小(AliasS); 返 1; }
    若 (大小(((AliasS *)0)->b) != 2) { 格式化 "FAIL ③ 别名反射 %d\n", 大小(((AliasS *)0)->b); 返 2; }
    若 (s.a + s.b[0] != 3) { 格式化 "FAIL 具名 struct 读写 %d\n", s.a + s.b[0]; 返 3; }
    若 (大小(AliasU) != 8) { 格式化 "FAIL ② 具名联合 %d (结构体化?)\n", 大小(AliasU); 返 4; }
    若 (大小(((AliasU *)0)->d) != 8) { 格式化 "FAIL 联合成员反射 %d\n", 大小(((AliasU *)0)->d); 返 5; }
    若 (大小(((AliasU *)0)->i) != 4) { 格式化 "FAIL 联合 int 成员 %d\n", 大小(((AliasU *)0)->i); 返 6; }
    格式化 "NAMED TAG %d %d %d %d %d %d\n", 大小(AliasS), 大小(((AliasS *)0)->b), s.a + s.b[0],
        大小(AliasU), 大小(((AliasU *)0)->d), 大小(((AliasU *)0)->i);
    返 0;
}
