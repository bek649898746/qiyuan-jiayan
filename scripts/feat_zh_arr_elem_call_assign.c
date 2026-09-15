// @EXPECTED out:ARR-ELEM-CALL OK
# 包含 <stdio.h>
/* P2-19 (2026-09-10): 结构体数组 元素 = 构值返回调用 — 8B 构值 (寄存器返回, 无 sret)
   修复前: 结构体整块拷贝分支 直接"取调用地址" → 运行 0xC0000005 (编译通过)
   修法: 该分支排除 调用 RHS → 落标量分支 (已有 sret/临时 机制) */
构 S { 整 a; 整 b; };
构 S 造(整 n) { 构 S x; x.a = n; x.b = n * 2; 返 x; }
函数 整 主(空) {
    构 S arr[3];
    整 i = 0;
    循环 (i < 3) { arr[i] = 造(i); i = i + 1; }
    若 (arr[2].a != 2 || arr[2].b != 4) { 格式化 "FAIL %d %d\n", arr[2].a, arr[2].b; 返 1; }
    格式化 "ARR-ELEM-CALL OK\n", 0;
    返 0;
}
