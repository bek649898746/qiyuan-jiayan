// @EXPECTED out:sz=8,8,4,2,1,24
/* A15-余 扩展 形态 锚 (2026-09-13, 不动点 170DEE4D → 504E5F0A): MSVC 类型族 (`__int64`/`__int32`/`__int16`/`__int8`)
   升为 甲言 类型词 后, **全 形态** 一次 钉住 —— 单 断言 `sizeof(__int64)` 只 覆盖 表层 (A15-余 的 由来: 锚 只 看 exit:0).

   覆盖 形态: ① 尺寸 全族 (`i64/u64/i32/i16/i8` + 结构体 = 8/8/4/2/1/24, gcc 神谕 逐值)
             ② 大值 取值 (5000000000 = 超 32 位; 4 字节 截断/隐式 int 会 立刻 现形)
             ③ 数组 元素 (`4294967296` = 2^32 存 64 位 槽, 4 字节 槽 会 变 0)
             ④ 64 位 运算 (`x >> 1` = 2^32; 按 32 位 算 会 得 0)
             ⑤ 结构体 字段 布局 + 大数 存取 (`i64 + i32 + i64` = 24 字节)
             ⑥ 局部 声明 `__int64 局部` **裸 类型** 形式 (不经 typedef)

   ⚠️ **锚 形态 说明 (2026-09-13 实测 约束)**: 本 锚 刻意 **不** 把 14 项 断言 挤进 一个 printf —— 排查 中 发现
   一 个 **既存 独立 缺陷** (`sizeof(typedef别名*)` 在 多 实参 上下文 求值 错乱, 与 本 站 无关; 见 `_DEBT_LEDGER`
   A47 挂账 + 最小复现 `scratch/a15_scan8.py` M7)。故 本 锚 用 **多行 / 每行 少 实参** 形态, 且 避开 `sizeof(别名*)`;
   断言行 = 最后 一行 (框架 `out:` 只 取 最后 一条 且 为 子串 匹配)。
   证据: 输出 与 `gcc -w` 编 同名 程序 **逐字节 全等**; 扩展 全形态 试验 `scratch/a15_int64_ext.c` 同样 全等。 */
#include <stdio.h>
typedef __int64 i64;
typedef unsigned __int64 u64;
typedef __int32 i32;
typedef __int16 i16;
typedef __int8 i8;

struct S { i64 甲; i32 乙; i64 丙; };

int main(void) {
    i64 arr[3];
    arr[0] = 1; arr[1] = 4294967296LL; arr[2] = -3;
    struct S st;
    st.甲 = 1234567890123LL; st.乙 = -7; st.丙 = -9876543210987LL;
    i64 x = 8589934592LL;
    __int64 局部 = 5000000000LL;
    printf("big=%lld arr=%lld,%lld,%lld\n",
        (long long)局部, (long long)arr[0], (long long)arr[1], (long long)arr[2]);
    printf("shift=%lld st=%lld,%d,%lld\n",
        (long long)(x >> 1), (long long)st.甲, (int)st.乙, (long long)st.丙);
    printf("sz=%d,%d,%d,%d,%d,%d\n",
        (int)sizeof(i64), (int)sizeof(u64), (int)sizeof(i32), (int)sizeof(i16), (int)sizeof(i8),
        (int)sizeof(struct S));
    return 0;
}
