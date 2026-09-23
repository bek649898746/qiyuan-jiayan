/* gate_gcc_link_driver.c — gcc 编译的驱动（链接甲言编出的 .obj）
 *
 * 期望输出（与「模块也用 gcc 编译」时逐字一致）:
 *   G1=428A2F98
 *   G2=0
 *   G3=1
 *   G4=1
 *   GATE-GCC-LINK OK
 */
#include <stdio.h>

int gate_tab(int i);
int gate_zero(void);
int gate_copy(void);
int gate_cmp(void);

int main(void) {
    printf("G1=%08X\n", (unsigned)gate_tab(0));
    printf("G2=%d\n", gate_zero());
    printf("G3=%d\n", gate_copy());
    printf("G4=%d\n", gate_cmp());
    if (gate_tab(0) != (int)0x428A2F98u) { printf("FAIL G1\n"); return 1; }
    if (gate_zero() != 0) { printf("FAIL G2\n"); return 2; }
    if (gate_copy() != 1) { printf("FAIL G3\n"); return 3; }
    if (gate_cmp() != 1)  { printf("FAIL G4\n"); return 4; }
    printf("GATE-GCC-LINK OK\n");
    return 0;
}
