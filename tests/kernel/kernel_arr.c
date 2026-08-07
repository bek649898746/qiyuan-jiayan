// 测试: 全局数组初始化
整 pos = 0;
静 无 字 garr[16] = { 0x7E, 0x5A, 0x5A, 0x5A, 0x7E, 0x5A, 0x5A, 0x7E, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00 };

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
    put_hex(garr[0]);
    put_hex(garr[15]);
    put_c(' ');
    put_hex(garr[4]);
    循环 (1) { }
}
