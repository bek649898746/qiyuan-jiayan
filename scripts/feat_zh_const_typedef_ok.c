// @EXPECTED out:CTD 6 5 7
// @EXPECTED exit:0
/* 第 455 站·第四十八轮 正锚: **只禁「const typedef 的写入」**，普通 typedef 与普通标量 照常可写。
   覆盖 三条 必须放行:
     · `CI x = 5;`（const typedef）⟹ **可读**
     · `TI y = 6; y = 6;`（普通 typedef）⟹ **可写**
     · `整 z = 0; z = 7;`（普通标量）⟹ 可写
   基准 (gcc 14.2.0): `CTD 6 5 7`。 */
typedef 整 常 CI;
typedef 整 TI;
函数 整 主(空) {
    CI x = 5;
    整 甲 = x;
    TI y = 6;
    y = 6;
    整 乙 = y;
    整 z = 0;
    z = 7;
    格式化 "CTD %d %d %d\n", 乙, 甲, z;
    若 (乙 != 6 || 甲 != 5 || z != 7) 返 1;
    返 0;
}
