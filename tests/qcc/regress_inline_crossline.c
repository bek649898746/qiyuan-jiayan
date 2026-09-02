/* 回归 (2026-09-02): inline 函数签名跨行 → zh 编译器曾永久卡死 (净集预扫描参数扫描 '\n' 死循环) */
static inline int add(int a,
    int b) { return a + b; }
static inline unsigned short pick(unsigned short lo,
    unsigned short hi,
    int sel) { return sel ? hi : lo; }
static inline long long sub(long long a,
    long long b) { return a - b; }
int main(void){
    return (int)(add(1,2) + pick(5,9,0) + (int)sub(10,3) == 15 ? 0 : 1);
}
