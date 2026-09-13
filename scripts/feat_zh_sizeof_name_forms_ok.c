// @EXPECTED out:i64x=8 int=4 gv=4 lv=4 arr=20 ea=4 sz=8 t2=1
/* A15-b 回归锚 (2026-09-14): `大小(<名字>)` 的 **全部合法 名字 形态** 必须 继续 通过 ——
   钉 住 新增 的 "名字 不存在 → A1001" 判据 **不 误伤** 下列 每 一 类:
     · i64x   typedef 名 (`型定判断` 在 L7487 已 分流)
     · int    类型词   (L7411 型词 分支)
     · gv/lv  全局/局部 变量 (变量表 命中)
     · arr    数组      (总字节 = 20)
     · EA     **枚举 常量** (落 L7538 词变量 分支 但 未 命中 变量表 —— 靠 `全回` 豁免)
     · sz     size_t 变量, t2 无符号 字符 变量
   落点 由 无条件探针 铁证: 名=EA 下=18 / 名=fun 下=17 / 名=NoSuchThing 下=18 —
   前 两 者 与 真·不存在 的 名字 走 **同 一 分支**, 故 判据 必须 同时 看
   「枚举常量表 (`全回`)」与「后 是否 跟 `(`」, 否则 会 误伤 枚举 常量 与 函数调用 形态.
   ⚠️ `大小(函(...))` (函数调用 形态) **本 锚 不 覆盖** —— 它 是 另一 独立 既有 缺陷
     (A67, 2026-09-14 发现): 词位 在 该 分支 后 残留 `(`, 产物 0xC0000005. seed 同样 崩,
     与 A15-b 改动 无关, 单独 挂账. */
#include <stdio.h>

typedef __int64 i64x;
enum E { EA = 1, EB = 2 };
int gv = 7;

int main(void) {
    int lv = 3;
    int arr[5];
    size_t sz = sizeof(arr);
    unsigned char t2 = 0;
    arr[0] = 1;
    t2 = (unsigned char)sizeof(char);
    printf("i64x=%d int=%d gv=%d lv=%d arr=%d ea=%d sz=%d t2=%d\n",
           (int)sizeof(i64x), (int)sizeof(int), (int)sizeof(gv), (int)sizeof(lv),
           (int)sizeof(arr), (int)sizeof(EA), (int)sizeof(sz), (int)sizeof(t2));
    return 0;
}
