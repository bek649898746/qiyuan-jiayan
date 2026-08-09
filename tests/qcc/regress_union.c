// @EXPECTED exit:0
// 回归: union 成员重叠 (2026-08-09 语言特性覆盖)
union U { int i; char c[4]; };
union U gu;
int main(void) {
    union U u;
    u.i = 0x12345678;
    if (u.c[0] != 0x78) return 1;
    if (u.c[1] != 0x56) return 2;
    if (u.c[3] != 0x12) return 3;
    gu.i = 0xAABBCCDD;
    if (gu.c[0] != 0xDD) return 4;
    if (gu.c[2] != 0xBB) return 5;
    return 0;
}
