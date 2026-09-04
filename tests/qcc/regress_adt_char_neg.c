// @EXPECTED compile_fail
/* R1-B 锤A (2026-09-04) #97 负例: 判别 载荷暂不支持 字符 (v1 标量集) → 编译期拦截 */
int main(void) {
    判别 Bad 于 { 点(字 键) };
    return 0;
}
