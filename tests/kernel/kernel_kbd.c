// 甲言裸机内核 - 键盘端口 IO 验证 (codegen 全权)
// 逻辑: 轮询 0x64 状态端口 bit0 (输出缓冲满) → 读 0x60 扫描码 → hex 回显 VGA
整 pos = 0;

空 put_c(整 c) {
    *(无 短*)(0xB8000 + pos * 2) = (无 短)(0x0200 | (无 短)c);
    pos++;
}

空 put_s(字 *s) {
    循环 (*s != 0) { put_c(*s); s++; }
}

空 put_hex(整 v) {
    整 hi = (v >> 4) & 0xF;
    整 lo = v & 0xF;
    若 (hi < 10) { put_c(hi + 48); } 否则 { put_c(hi - 10 + 65); }
    若 (lo < 10) { put_c(lo + 48); } 否则 { put_c(lo - 10 + 65); }
}

空 _start(空) {
    put_s("KBD:");
    循环 (1) {
        整 s = inb(0x64);
        若 ((s & 1) != 0) {
            整 sc = inb(0x60);
            put_hex(sc);
        }
    }
}
