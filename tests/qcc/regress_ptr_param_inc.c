// @EXPECTED exit:0
// 回归: 指针参数后置自增后解引用 (BUG-NEW-1, fix 2026-08-09)
// 根因: var_param 指针参数 p_esz 硬编码 4 → char* a++ 步进 4 字节(应 1)
// 影响: 字符串遍历模式 while(*p){...p++;} 全错 (strcmp/strlen/解析器)
int f(const char* a) { a++; return *a; }
int g(const char* a) { int c = 0; while (*a) { c++; a++; } return c; }
int main(void) {
    if (f("abc") != 98) return 1;      /* 'b' */
    if (g("hello") != 5) return 2;     /* strlen=5 */
    if (f("xyz") != 121) return 3;     /* 'y' */
    return 0;
}
