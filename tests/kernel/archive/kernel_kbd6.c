// 隔离: if 嵌套内 变量参数调用 (put_c(sc))
整 pos = 0;

空 put_c(整 c) {
    *(无 短*)(0xB8000 + pos * 2) = (无 短)(0x0200 | (无 短)c);
    pos++;
}

空 _start(空) {
    put_c('K');
    循环 (1) {
        整 s = inb(0x64);
        若 ((s & 1) != 0) {
            整 sc = inb(0x60);
            put_c(sc);
        }
    }
}
