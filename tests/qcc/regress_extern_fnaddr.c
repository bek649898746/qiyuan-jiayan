// @KNOWN-GAP: 应拒面缺失 —— 本负例锚 实测【编译成功】, 闸的 compile_fail 分支从未触达 ⟹ 其「应拒」语义一直未被检测 (2026-09-26 round 29 审计发现)
// @EXPECTED compile_fail
// fix 2026-08-12: extern 未定义函数名作值 = 编译期诊断 (单文件模型无符号可解析)
int printf(const char*, ...);
extern int bar(int);
int main() {
    int z = bar;   // 应编译报错
    printf("z=%d\n", z);
    return 0;
}
