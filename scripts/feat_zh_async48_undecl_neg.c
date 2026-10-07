// @EXPECTED compile_fail
// @EXPECTED err:A6074
#包含 <stdio.h>
/* #48 负锚 (R539): 生成器 步进 到 **未声明** 生成器 ⟹ **A6074** —— 覆盖 拒面 (原 闸内 只有 正锚 feat_zh_async20.c).
   形态: `下一个(g2, v)` 而 `g2` 从未声明 (异步任务 = 生成器 fiber; 未声明 实例 必 响亮拒绝, 不得 静默).
   修复前 (父编 `C3AA4836`): **rc=0 编译通过 + 运行期 0xC0000005**（合成 `&g2` 取 垃圾地址）⟹ 本锚 在 父编 必红。 */
int main(void) {
    long long v = 0;
    下一个(g2, v);
    return 0;
}
