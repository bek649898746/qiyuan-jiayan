// @EXPECTED compile_fail
// @EXPECTED exit:nonzero
// @EXPECTED err:A1001
/* A15-b 负锚 (2026-09-14): `大小(<完全 不存在 的 名字>)` 必须 **响亮 报错**.
   修 前: 落 `大小` 的 词变量 分支 (L7538) → 遍 变量表 未 命中 → `命中一 = 0`
     → `若 (!命中一)` 只 处理 "已注册 裸标签" (T1) 与 类型名 (L7487 分流)
     → 其余 静默 保持 `整 尺寸 = 4;` 并 返 出 ⟹ `sizeof(拼错的名字)` = 4.
   危害 同 T1/A54: 假尺寸 进 `分配内存(n * 大小(X))` = 堆溢出 炸弹, 且 用户 无 任何 信号.
   神谕: gcc 报 `error: '<name>' undeclared`. */
#include <stdio.h>

int main(void) {
    printf("%d\n", (int)sizeof(NoSuchThingAtAll));
    return 0;
}
