// 最小复现: _start + inb(0x64) 赋值
整 pos = 0;

空 put_c(整 c) {
    *(无 短*)(0xB8000 + pos * 2) = (无 短)(0x0200 | (无 短)c);
    pos++;
}

空 _start(空) {
    put_c('K');
    整 s = inb(0x64);
    put_c('S');
    循环 (1) { }
}
