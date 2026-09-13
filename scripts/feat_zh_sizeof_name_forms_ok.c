// @EXPECTED out:i64x=8 int=4 gv=4 lv=4 arr=20 ea=4 sz=8 t2=1 call=4 ccall=1 dcall=8 fcall=4
/* A15-b + A67 回归锚 (2026-09-14): `大小(<名字>)` 的 **全部合法 形态** 必须 通过, 且 **不 误伤**.
   钉 住 新增 的 "名字 不存在 → A1001" 判据 不 误伤 每 一 类 合法 名字:
     · i64x   typedef 名 (`型定判断` 在 上方 已 分流)
     · int    类型词
     · gv/lv  全局/局部 变量 (变量表 命中)
     · arr    数组 (总字节 20)
     · EA     **枚举 常量** (落 词变量 分支 但 未 命中 变量表 —— 靠 `全回` 豁免)
     · sz     size_t 变量, t2 无符号 字符 变量
   落点 由 无条件探针 铁证: 名=EA 下=18 / 名=fun 下=17 / 名=NoSuchThing 下=18 —— 三者 走 **同一分支**
   ⟹ 判据 必须 同时 看「枚举常量表 (`全回`)」与「后 是否 跟 `(`」.

   ==== A67 (2026-09-14, 同 锚 覆盖) — `大小(函(...))` 产物 0xC0000005 ====
   病灶: 词变量 分支 消费 名字 后 只 消费 **成员链** 与 **下标**, **不 消费 实参表 `(`** → 词位 残留 → 解析 错乱 →
     产物 **访问违规 且 无 输出** (未改动 的 seed 同 样 崩, 属 既有 缺陷).
   修法 (根治): ① 后 跟 `(` ⟹ 消费 括号平衡 的 实参表, 尺寸 = 函数 **返回 类型** 宽度;
     ② 新增 **解析期** 返回 宽度 表 `返宽表` (主循环 解析 函数头 时 登记) —— 既有 `中细`/`半次一` 与
        `源己`/`当三` **都 只 在 码生成器 阶段** 登记, 解析期 查 恒 0, 故 单列 一 表.
     ③ 判据 加 `查函数纯 < 0` 豁免 —— 首版 漏 了 `大小(函数名)` (无 调用) 形态, 会 误报 "未声明"
        (闸 当时 无 此 锚, 未 拦下 —— **闸 盲区**, 本 锚 补).
   覆盖: call (int 返回) / ccall (字符 返回 1) / dcall (双精度 返回 8) / **fcall (单精度 返回 4, A68)** —— 与 gcc **逐字节全等**.
   ✅ A68 已闭环 (2026-09-14 同日): 解析函数头时的 `函返浮` **不区分 float/double** (float 返回 = double xmm0 值语义, 码生成器需要),
     故 首版 返宽表 对 `float f(void)` 也 记 8 (应 4) ⟹ 新增 `函返单` 标记 (类型词 `单`/`float` 处 + 类型定义 浮标且尺寸==4 处),
     `登返宽` 里 `若 (函返单) 4 否 若 (函返浮) 8`. */
#include <stdio.h>

typedef __int64 i64x;
enum E { EA = 1, EB = 2 };
int gv = 7;
int fun(int a) { return a + 1; }
char cfun(void) { return 65; }
double dfun(void) { return 1.5; }
float ffun(void) { return 1.5f; }

int main(void) {
    int lv = 3;
    int arr[5];
    size_t sz = sizeof(arr);
    unsigned char t2 = 0;
    arr[0] = 1;
    t2 = (unsigned char)sizeof(char);
    printf("i64x=%d int=%d gv=%d lv=%d arr=%d ea=%d sz=%d t2=%d call=%d ccall=%d dcall=%d fcall=%d\n",
           (int)sizeof(i64x), (int)sizeof(int), (int)sizeof(gv), (int)sizeof(lv),
           (int)sizeof(arr), (int)sizeof(EA), (int)sizeof(sz), (int)sizeof(t2),
           (int)sizeof(fun(lv)), (int)sizeof(cfun()), (int)sizeof(dfun()), (int)sizeof(ffun()));
    return 0;
}
