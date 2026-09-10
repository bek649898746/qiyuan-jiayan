// @EXPECTED out:DISC VALUE OK
# 包含 <stdio.h>
/* P3-17 (2026-09-10): 判别 sum type 构值 — 变体构造 (设 tag + 本变体载荷) 后 值拷贝/传递 不得误判部分初始化
   修复前: 判别构型 struct {tag,p0,p1} 计入"需全写"字段 → 只设置 tag+p0 后 整体拷贝 → 假阳 A4001
   (判别 idiom: 其余载荷槽 按变体语义 无关) */
判别 形状 于 { 圆(r), 方(e), 点() };
函数 整 主(空) {
    struct 形状 c;
    c.tag = 形状_圆; c.p0 = 5;
    struct 形状 d = c;                 /* 判别 值 拷贝 */
    若 (d.tag != 形状_圆 || d.p0 != 5) { 格式化 "FAIL 拷贝\n", 0; 返 1; }
    整 s = 0;
    若 (d.tag == 形状_圆) s = d.p0 * d.p0;
    否 若 (d.tag == 形状_方) s = d.p0;
    若 (s != 25) { 格式化 "FAIL 判别 s=%d\n", s; 返 2; }
    struct 形状 e;
    e.tag = 形状_点; e.p0 = 0;
    若 (e.tag != 形状_点) 返 3;
    格式化 "DISC VALUE OK\n", 0;
    返 0;
}
