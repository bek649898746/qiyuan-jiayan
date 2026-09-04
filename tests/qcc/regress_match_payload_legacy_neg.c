// @EXPECTED compile_fail
/* R1-B 锤B (2026-09-04) #98 负例2: 非类型化 (旧式) 判别 用 变体载荷臂 → 明确拦截 (非静默) */
int main(void) {
    判别 Old 于 { 甲(值), 乙() };
    struct Old o;
    o.tag = Old_甲; o.p0 = 3;
    整 r;
    依据 r, o.tag 穷尽 Old 对照 甲(值): 1, 乙(): 0;
    return 0;
}
