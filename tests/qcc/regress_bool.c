// @EXPECTED exit: 2
// Phase 2-3: _Bool 1字节布尔
int main(void) {
    _Bool b = 1;
    _Bool c = 0;
    if (b) { c = 1; }
    return b + c; /* 1+1 = 2 */
}
