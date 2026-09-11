#include <stdint.h>
// @EXPECTED exit:2  /* 负例/自证: sizeof(int16_t)=2 经 返回 值 传 exit code (地1 G0-1 意图标注 2026-09-11) */
int main(void) {
    int16_t x = 5;
    int r = sizeof(x);
    return r;
}
