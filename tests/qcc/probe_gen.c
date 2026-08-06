// @EXPECTED exit:0
static struct { int is_param; int pstk; int p_esz; int preg; int pdisp; int rsp_off; } vars[16];
static int vcnt, cur_frame_sz;
static void f(int x, int y) {}
static void g(int x) {}
static void gen(void) {
    for (int vi = 0; vi < vcnt; vi++) {
        if (vars[vi].is_param) {
            if (vars[vi].pstk) {
                if (vars[vi].p_esz > 0) { f(vars[vi].pdisp, 0); }
                else { g(vars[vi].rsp_off - cur_frame_sz); }
            } else if (vars[vi].p_esz > 0) f(vars[vi].rsp_off - cur_frame_sz, vars[vi].preg);
            else g(vars[vi].preg);
        }
    }
}
int main(void) { return 0; }
