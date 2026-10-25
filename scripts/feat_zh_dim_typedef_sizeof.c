// @EXPECTED silent:0
/* feat_zh_dim_typedef_sizeof.c — R800b 载体 (2026-10-23 虾米)
 *
 * `typedef 构 Key Key;` 在 结构体 定义 **之前** 登记 ⟹ 型定表 尺寸 记 0 (快照)。
 * 结构体 定义 后 `尺寸(Key)` 必须 得 现算 值 8 (原: 0 / 误报 A1001 不完整)。
 * 现场: sqlite3.c `struct KeyInfo` 一族。
 *
 * 断言 = 布局值: buf[8] ⟹ sizeof(struct Holder) == 8.
 */
typedef struct Key Key;

struct Key {
    int a;
    int b;
};

struct Holder {
    char buf[sizeof(Key)];
};

int main(void) { return sizeof(struct Holder) == 8 ? 0 : 1; }
