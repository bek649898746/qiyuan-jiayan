// @EXPECTED exit:0
// 回归: 全局 字节 变量 (fix 2026-08-09)
// 根因: 镜像 lexer 缺 否则->else 映射 (宿主 L2482 有 || 否则, 镜像只映射 否)
//       → 字节 行的 否则若 在 v2 里不是 else-if → 词法映射自不一致 → 全局字节 codegen 崩
字节 gbuf[8];
int main(void) {
    int i;
    for (i = 0; i < 8; i++) gbuf[i] = (字节)(i + 65);
    if (gbuf[0] != 65) return 1;
    if (gbuf[7] != 72) return 2;
    if (sizeof(gbuf) != 8) return 3;
    return 0;
}
