// 最小复现: _start + 局部变量赋值 (无 inb)
整 pos = 0;

空 put_c(整 c) {
    *(无 短*)(0xB8000 + pos * 2) = (无 短)(0x0200 | (无 短)c);
    pos++;
}

空 put_s(字 *s) {
    循环 (*s != 0) { put_c(*s); s++; }
}

空 _start(空) {
    put_s("KBD:");
    整 x = 5;
    put_c(x + 48);
    循环 (1) { }
}
