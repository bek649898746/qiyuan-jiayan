// @EXPECTED silent:0
/* feat_zh_union_dim_typedef.c — R800 载体 (2026-10-23 虾米)
 *
 * 从 sqlite3.c `BITVEC_USIZE` 缩出：`尺寸(<不完整 typedef> *)` 出现在 字段 数组维度 里。
 *   根因: 常量 求值器 的 `尺寸(类型定义)` 分支 —— 型定表 尺寸 == 0 (前向 声明 快照) 时
 *   **不消费词位** ⟹ 维度 解析 早停 ⟹ 外层 字段循环 把 类型名/后续 sizeof 当 字段名
 *   (静默 错值 + A9001 误报)。
 *
 * 断言 = 布局值 (不只是「能编译」): int 头 4B + union{ 512B } ⟹ 512+4 对齐 8 = 520.
 */
typedef struct Node Node;

struct Bit {
    int head;
    union {
        unsigned char a[(512 / sizeof(Node *)) * sizeof(Node *)];
    } u;
};

int main(void) { return sizeof(struct Bit) == 520 ? 0 : 1; }
