// @EXPECTED exit: 3
// Phase 2-3: _Bool 全局 + 数组 (1字节元素)
_Bool gb = 1;
int main(void) {
    _Bool arr[4] = {1, 0, 1, 0};
    return gb + arr[0] + arr[2]; /* 1+1+1 = 3 */
}
