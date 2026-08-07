// @EXPECTED exit:0
// 回归: 静态 struct 数组 ≤8B 元素作值 (fix 2026-08-07 镜像 case-14)
//   C 版 08-06: is_struct_elem esz<=8 → 解引用取值
//   镜像该分支为空 → v1/v2 编此程序 值赋到地址而非元素值
//   注: 用逐字段初始化绕过全局 struct 数组 brace 初始化 bug (独立问题, 另修)
struct Pair { int a; int b; };   /* 8 字节 struct */
static struct Pair pairs[3];
int main() {
    pairs[0].a = 1; pairs[0].b = 2;
    pairs[1].a = 3; pairs[1].b = 4;
    pairs[2].a = 5; pairs[2].b = 6;
    struct Pair p = pairs[1];    /* 8B struct 元素整体作值 → 镜像旧代码留地址 */
    if (p.a != 3 || p.b != 4) return 1;
    return 0;
}
