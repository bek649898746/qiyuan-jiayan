// 载体 B（P0）：具名 union 嵌在 struct 里，字段数组维度含 sizeof ⟹ 当前编译器**挂起**（>60s 死循环）。
// 期望：20 秒内 rc=0。
// @EXPECTED silent:0
struct S {
  union U {
    int a[(8 / sizeof(int)) * sizeof(int)];
  } u;
};

int main(void) { return 0; }
