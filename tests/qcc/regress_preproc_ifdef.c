// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
#include <string.h>
/* 回归: 预处理器条件编译 #ifdef/#ifndef/#if/#elif/#else/#endif + defined() */
#define VER 3
#define FEAT_X

#if defined(VER) && VER > 2
#define TAG "A"
#elif VER == 2
#define TAG "B"
#else
#define TAG "C"
#endif

#ifdef FEAT_X
#define HASX 1
#else
#define HASX 0
#endif

#ifndef FEAT_Y
#define HASY 0
#endif

#define POS_TEST 5
#if POS_TEST > 2
#define ISPOS 1
#else
#define ISPOS 0
#endif

int main(void) {
    if (strcmp(TAG, "A") != 0) return 1;  /* #if defined+比较 → A */
    if (HASX != 1) return 2;          /* #ifdef FEAT_X → 1 */
    if (HASY != 0) return 3;          /* #ifndef FEAT_Y → 0 */
    if (ISPOS != 1) return 4;         /* #if 宏数值比较 → 1 */
    /* #if 与运算 */
#if defined(FEAT_X) && !defined(FEAT_Z)
    printf("PASS\n");
#else
    return 5;
#endif
    return 0;
}
