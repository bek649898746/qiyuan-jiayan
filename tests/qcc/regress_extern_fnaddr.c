// @EXPECTED compile_fail
// fix 2026-08-12: extern 未定义函数名作值 = 编译期诊断 (单文件模型无符号可解析)
int printf(const char*, ...);
extern int bar(int);
int main() {
    int z = bar;   // 应编译报错
    printf("z=%d\n", z);
    return 0;
}
