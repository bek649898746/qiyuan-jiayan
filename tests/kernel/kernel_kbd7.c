// 测试: put_hex 无否则 (纯 if)
整 pos = 0;

空 put_c(整 c) {
    *(无 短*)(0xB8000 + pos * 2) = (无 短)(0x0200 | (无 短)c);
    pos++;
}

空 put_hex(整 v) {
    整 hi = (v >> 4) & 0xF;
    整 lo = v & 0xF;
    若 (hi < 10) { put_c(hi + 48); }
    若 (hi >= 10) { put_c(hi - 10 + 65); }
    若 (lo < 10) { put_c(lo + 48); }
    若 (lo >= 10) { put_c(lo - 10 + 65); }
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
