// 载体 A（从 sqlite3.c struct Bitvec 缩出）：匿名 union 的 **char** 数组，维度是含 sizeof 的乘式。
// 期望：rc=0。当前红：A9001 字段名 'sizeof' 是保留词（R472 误报）。
// @EXPECTED silent:0
struct S {
  union {
    unsigned char a[(512 / sizeof(unsigned int)) * sizeof(unsigned int)];
  } u;
};

int main(void) { return 0; }
