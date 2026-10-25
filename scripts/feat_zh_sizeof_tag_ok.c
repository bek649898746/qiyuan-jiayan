// @EXPECTED out:SIZEOF TAG 12 8 8 12
#include <stdio.h>

/* T1 (2026-09-13) 正锚: 带 `构`/`联` 词头 的 大小 必须 照常 工作 (与 负锚 配对, 保证
 *   零静默 修复 不 误伤 合法 形态)。同时钉死 与 gcc 一致 的 基准值:
 *   构 P3 = 3×4 = 12; 构 P3 * = 8; 联 U = max(4,8) = 8; 类型定义 别名 = 12。
 *   gcc 基准: sizeof(struct P3)=12, sizeof(struct P3*)=8, sizeof(union U)=8。
 *   注: 甲言 的 sizeof 关键字 是 `大小` (逆低一 L5034) —— `尺寸` **不是** 关键字 (写 `尺寸(x)` 会 报
 *       「未声明函数 '尺寸'」, 响亮 但 非 本 特性 语义)。
 * @EXPECTED out:SIZEOF TAG 12 8 8 12 */

构 P3 { 整 a; 整 b; 整 c; };
联 U { 整 i; 双 d; };
类型定义 构 P3 P3T;

函数 整 主(空) {
    格式化 "SIZEOF TAG %d %d %d %d\n", 大小(构 P3), 大小(构 P3 *), 大小(联 U), 大小(P3T);
    返 0;
}
