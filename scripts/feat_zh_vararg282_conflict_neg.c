// @EXPECTED compile_fail
// @EXPECTED err:A6109
/* #282 边界负锚（2026-10-18 · 300 轮第四刀）：
   同名「变参 + 非变参」重载必须 **响亮拒绝**（A6109），不得静默走首条/错路。
   修前：`pick(int,int)` 与 `pick(int,...)` 同时存在时语义不确定；
   修后：`查函数参` 对变参复用首条，且同参重复定义仍 A6109 ⟹ 该组合被明确拒绝。 */
static int pick(int a, int b) { return a + b; }
static int pick(int n, ...);
static int pick(int n, ...) { return n * 100; }
int main(void) { return pick(1, 2) + pick(3, 4, 5); }
