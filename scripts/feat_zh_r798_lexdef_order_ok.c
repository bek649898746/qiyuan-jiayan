// @EXPECTED out:ANCHOR 0
// @EXPECTED exit:0
#包含 <stdio.h>
/* R798 (2026-10-13 虾米 · 语料攻关闭环 · D2 诊断相位族): 词法阶段 #ifdef/#ifndef **时序语义**。

   病根: 词法阶段条件编译判定(L7449)用 邻细() 查三张全表 —— 其中 `函数宏表` 是 行申()
   在**词法前全文预收集**的 ⟹ 早于其定义处的 `#ifndef <函数宏>` 被判「已定义」
   ⟹ 走 #else 支 ⟹ `MAKELIST` 未定义 ⟹ `#ifdef MAKELIST` 判假 ⟹ 剥掉块内 `typedef enum {`
   ⟹ 枚举成员 `V0,V1,...` 散落到文件作用域 ⟹ 后续无花括号 `if` 落顶层
   ⟹ 撞「函数名是语句关键字」诊断(假 A9001)。libjpeg/libpng D2 族由此而来(libjpeg 53/93→65/93)。

   本锚: guard 名 `MM` 在**后文**才 `#define MM(a) a`(函数宏)。
   修前(旧引擎 r715i): MM 被误判已定义 ⟹ MAKELIST 未定义 ⟹ enum 被剥 ⟹ 编译失败(**必红**)。
   修后: MAKELIST 已定义 ⟹ enum 保留 ⟹ 全绿。
   判别对照: 把首 6 行守卫换成 `#define MAKELIST`(plain 头)⟹ 旧引擎也绿 ⟹ 本形态专测时序语义。 */
#ifndef MM
#ifndef GG
#define MAKELIST
#else
#define MM(a)
#endif
#endif
#ifdef MAKELIST
typedef enum {
#define MM(a)  a,
#endif
MM(V0)
MM(V1)
MM(V2)
MM(V3)
#ifdef MAKELIST
} E;
#endif
#define MM(a) a
函数 整 主(空) {
    E 甲 = V0;
    若 ((整)甲 != 0) 返 1;
    格式化 "ANCHOR %d\n", (整)甲;
    返 0;
}
