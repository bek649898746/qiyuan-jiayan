// @EXPECTED compile_fail
// @EXPECTED err:A1001
#包含 <stdio.h>
/* #47 负锚 (R538): 投递 到 **未声明** 邮箱 ⟹ 回退 actor_send ⟹ A1001 (未声明函数) —— 覆盖 拒面 (原 闸内 只有 正锚). */
int main(void) {
    投递 未声明箱, 1;
    return 0;
}
