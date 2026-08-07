// 甲言裸机内核 - 多字符输出 (全局 pos 递增)
整 pos = 0;  // VGA 光标

空 put_c(整 c) {
    *(无 短*)(0xB8000 + pos * 2) = (无 短)(0x0200 | (无 短)c);
    pos++;
}

空 _start(空) {
    put_c('J');
    put_c('Y');
    put_c('!');
    put_c(' ');
    put_c('8');
    put_c('2');
    put_c('8');
    循环 (1) { }
}
