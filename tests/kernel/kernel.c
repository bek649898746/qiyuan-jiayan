// 甲言裸机内核 - 字符串输出 (codegen 全权)
整 pos = 0;

空 put_c(整 c) {
    *(无 短*)(0xB8000 + pos * 2) = (无 短)(0x0200 | (无 短)c);
    pos++;
}

空 put_s(字 *s) {
    循环 (*s != 0) {
        put_c(*s);
        s++;
    }
}

空 _start(空) {
    put_s("JY!");
    put_c(' ');
    put_s("828");
    循环 (1) { }
}
