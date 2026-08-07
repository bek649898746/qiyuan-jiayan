// 甲言裸机内核 v3 - VGA 自定义字库 + 中文输出 + 键盘回显
// 中文: 黑体 8x16 点阵加载到 VGA plane 2 (字库平面), 字符码 0x80-0x88
整 pos = 0;

空 put_c(整 c) {
    *(无 短*)(0xB8000 + pos * 2) = (无 短)(0x0200 | (无 短)c);
    pos++;
}

空 put_s(字 *s) {
    循环 (*s != 0) { put_c(*s); s++; }
}

空 put_nl(空) {
    pos = ((pos / 80) + 1) * 80;
}

空 font_row(整 slot, 整 row, 整 v) {
    *(无 字*)(0xA0000 + slot * 16 + row) = v;
}

空 load_font(空) {
    // VGA 序列: 禁用奇偶寻址 → 选 plane 2 → 解锁 plane 2 写入
    outb(0x3CE, 0x05); outb(0x3CF, 0x40);
    outb(0x3C4, 0x04); outb(0x3C5, 0x02);
    outb(0x3C4, 0x02); outb(0x3C5, 0x04);
    // 清空 256 字符字库区 (4096 字节)
    整 i = 0;
    循环 (i < 4096) {
        *(无 字*)(0xA0000 + i) = 0;
        i++;
    }
    // 汉字字模 (槽位 0x80 起)
    font_row(128, 0, 0x7E);  // 0x80=甲
    font_row(128, 1, 0x5A);  // 0x80=甲
    font_row(128, 2, 0x5A);  // 0x80=甲
    font_row(128, 3, 0x5A);  // 0x80=甲
    font_row(128, 4, 0x7E);  // 0x80=甲
    font_row(128, 5, 0x5A);  // 0x80=甲
    font_row(128, 6, 0x5A);  // 0x80=甲
    font_row(128, 7, 0x7E);  // 0x80=甲
    font_row(128, 8, 0x7E);  // 0x80=甲
    font_row(128, 9, 0x18);  // 0x80=甲
    font_row(128, 10, 0x18);  // 0x80=甲
    font_row(128, 11, 0x18);  // 0x80=甲
    font_row(128, 12, 0x18);  // 0x80=甲
    font_row(128, 13, 0x18);  // 0x80=甲
    font_row(128, 14, 0x00);  // 0x80=甲
    font_row(128, 15, 0x00);  // 0x80=甲
    font_row(129, 0, 0x10);  // 0x81=言
    font_row(129, 1, 0xFF);  // 0x81=言
    font_row(129, 2, 0x00);  // 0x81=言
    font_row(129, 3, 0x7E);  // 0x81=言
    font_row(129, 4, 0x00);  // 0x81=言
    font_row(129, 5, 0x00);  // 0x81=言
    font_row(129, 6, 0x7E);  // 0x81=言
    font_row(129, 7, 0x00);  // 0x81=言
    font_row(129, 8, 0x7E);  // 0x81=言
    font_row(129, 9, 0x42);  // 0x81=言
    font_row(129, 10, 0x42);  // 0x81=言
    font_row(129, 11, 0x7E);  // 0x81=言
    font_row(129, 12, 0x42);  // 0x81=言
    font_row(129, 13, 0x00);  // 0x81=言
    font_row(129, 14, 0x00);  // 0x81=言
    font_row(129, 15, 0x00);  // 0x81=言
    font_row(130, 0, 0x18);  // 0x82=内
    font_row(130, 1, 0x18);  // 0x82=内
    font_row(130, 2, 0x18);  // 0x82=内
    font_row(130, 3, 0x7E);  // 0x82=内
    font_row(130, 4, 0x52);  // 0x82=内
    font_row(130, 5, 0x52);  // 0x82=内
    font_row(130, 6, 0x5A);  // 0x82=内
    font_row(130, 7, 0x5E);  // 0x82=内
    font_row(130, 8, 0x76);  // 0x82=内
    font_row(130, 9, 0x66);  // 0x82=内
    font_row(130, 10, 0x42);  // 0x82=内
    font_row(130, 11, 0x42);  // 0x82=内
    font_row(130, 12, 0x42);  // 0x82=内
    font_row(130, 13, 0x46);  // 0x82=内
    font_row(130, 14, 0x00);  // 0x82=内
    font_row(130, 15, 0x00);  // 0x82=内
    font_row(131, 0, 0x44);  // 0x83=核
    font_row(131, 1, 0x5F);  // 0x83=核
    font_row(131, 2, 0xE8);  // 0x83=核
    font_row(131, 3, 0xE8);  // 0x83=核
    font_row(131, 4, 0x4A);  // 0x83=核
    font_row(131, 5, 0x5E);  // 0x83=核
    font_row(131, 6, 0x76);  // 0x83=核
    font_row(131, 7, 0xEE);  // 0x83=核
    font_row(131, 8, 0xCA);  // 0x83=核
    font_row(131, 9, 0x5E);  // 0x83=核
    font_row(131, 10, 0x46);  // 0x83=核
    font_row(131, 11, 0x4F);  // 0x83=核
    font_row(131, 12, 0x59);  // 0x83=核
    font_row(131, 13, 0x00);  // 0x83=核
    font_row(131, 14, 0x00);  // 0x83=核
    font_row(131, 15, 0x00);  // 0x83=核
    font_row(132, 0, 0x08);  // 0x84=启
    font_row(132, 1, 0x7E);  // 0x84=启
    font_row(132, 2, 0x42);  // 0x84=启
    font_row(132, 3, 0x42);  // 0x84=启
    font_row(132, 4, 0x7E);  // 0x84=启
    font_row(132, 5, 0x7E);  // 0x84=启
    font_row(132, 6, 0x40);  // 0x84=启
    font_row(132, 7, 0x7E);  // 0x84=启
    font_row(132, 8, 0x62);  // 0x84=启
    font_row(132, 9, 0x62);  // 0x84=启
    font_row(132, 10, 0x62);  // 0x84=启
    font_row(132, 11, 0xFE);  // 0x84=启
    font_row(132, 12, 0xA2);  // 0x84=启
    font_row(132, 13, 0x22);  // 0x84=启
    font_row(132, 14, 0x00);  // 0x84=启
    font_row(132, 15, 0x00);  // 0x84=启
    font_row(133, 0, 0x04);  // 0x85=动
    font_row(133, 1, 0x04);  // 0x85=动
    font_row(133, 2, 0x74);  // 0x85=动
    font_row(133, 3, 0x04);  // 0x85=动
    font_row(133, 4, 0x0F);  // 0x85=动
    font_row(133, 5, 0xF5);  // 0x85=动
    font_row(133, 6, 0xF5);  // 0x85=动
    font_row(133, 7, 0x45);  // 0x85=动
    font_row(133, 8, 0x75);  // 0x85=动
    font_row(133, 9, 0x55);  // 0x85=动
    font_row(133, 10, 0x5D);  // 0x85=动
    font_row(133, 11, 0xFB);  // 0x85=动
    font_row(133, 12, 0x0B);  // 0x85=动
    font_row(133, 13, 0x1E);  // 0x85=动
    font_row(133, 14, 0x00);  // 0x85=动
    font_row(133, 15, 0x00);  // 0x85=动
    font_row(134, 0, 0x00);  // 0x86=。
    font_row(134, 1, 0x00);  // 0x86=。
    font_row(134, 2, 0x00);  // 0x86=。
    font_row(134, 3, 0x00);  // 0x86=。
    font_row(134, 4, 0x00);  // 0x86=。
    font_row(134, 5, 0x00);  // 0x86=。
    font_row(134, 6, 0x00);  // 0x86=。
    font_row(134, 7, 0x00);  // 0x86=。
    font_row(134, 8, 0x00);  // 0x86=。
    font_row(134, 9, 0x00);  // 0x86=。
    font_row(134, 10, 0x60);  // 0x86=。
    font_row(134, 11, 0x60);  // 0x86=。
    font_row(134, 12, 0x60);  // 0x86=。
    font_row(134, 13, 0x00);  // 0x86=。
    font_row(134, 14, 0x00);  // 0x86=。
    font_row(134, 15, 0x00);  // 0x86=。
    font_row(135, 0, 0x64);  // 0x87=种
    font_row(135, 1, 0xA4);  // 0x87=种
    font_row(135, 2, 0x3F);  // 0x87=种
    font_row(135, 3, 0x3D);  // 0x87=种
    font_row(135, 4, 0xFD);  // 0x87=种
    font_row(135, 5, 0x7D);  // 0x87=种
    font_row(135, 6, 0x7D);  // 0x87=种
    font_row(135, 7, 0x7F);  // 0x87=种
    font_row(135, 8, 0x7D);  // 0x87=种
    font_row(135, 9, 0xFD);  // 0x87=种
    font_row(135, 10, 0xA4);  // 0x87=种
    font_row(135, 11, 0x24);  // 0x87=种
    font_row(135, 12, 0x24);  // 0x87=种
    font_row(135, 13, 0x24);  // 0x87=种
    font_row(135, 14, 0x00);  // 0x87=种
    font_row(135, 15, 0x00);  // 0x87=种
    font_row(136, 0, 0x7E);  // 0x88=子
    font_row(136, 1, 0x06);  // 0x88=子
    font_row(136, 2, 0x0C);  // 0x88=子
    font_row(136, 3, 0x18);  // 0x88=子
    font_row(136, 4, 0x18);  // 0x88=子
    font_row(136, 5, 0xFF);  // 0x88=子
    font_row(136, 6, 0xFE);  // 0x88=子
    font_row(136, 7, 0x18);  // 0x88=子
    font_row(136, 8, 0x18);  // 0x88=子
    font_row(136, 9, 0x18);  // 0x88=子
    font_row(136, 10, 0x18);  // 0x88=子
    font_row(136, 11, 0x18);  // 0x88=子
    font_row(136, 12, 0x30);  // 0x88=子
    font_row(136, 13, 0x00);  // 0x88=子
    font_row(136, 14, 0x00);  // 0x88=子
    font_row(136, 15, 0x00);  // 0x88=子
    // 恢复: 所有 plane 写使能 → 恢复奇偶 → 恢复 Map Mask
    outb(0x3C4, 0x02); outb(0x3C5, 0x0F);
    outb(0x3CE, 0x05); outb(0x3CF, 0x10);
    outb(0x3C4, 0x04); outb(0x3C5, 0x03);
}

// Set 1 扫描码(通码) → ASCII; 未知返 0
整 sc_to_ascii(整 sc) {
    若 (sc == 0x1E) { 返 97; }   // a
    若 (sc == 0x1F) { 返 115; }  // s
    若 (sc == 0x20) { 返 100; }  // d
    若 (sc == 0x21) { 返 102; }  // f
    若 (sc == 0x22) { 返 103; }  // g
    若 (sc == 0x23) { 返 104; }  // h
    若 (sc == 0x24) { 返 106; }  // j
    若 (sc == 0x25) { 返 107; }  // k
    若 (sc == 0x26) { 返 108; }  // l
    若 (sc == 0x2C) { 返 122; }  // z
    若 (sc == 0x2D) { 返 120; }  // x
    若 (sc == 0x2E) { 返 99; }   // c
    若 (sc == 0x2F) { 返 118; }  // v
    若 (sc == 0x30) { 返 98; }   // b
    若 (sc == 0x31) { 返 110; }  // n
    若 (sc == 0x32) { 返 109; }  // m
    若 (sc == 0x10) { 返 113; }  // q
    若 (sc == 0x11) { 返 119; }  // w
    若 (sc == 0x12) { 返 101; }  // e
    若 (sc == 0x13) { 返 114; }  // r
    若 (sc == 0x14) { 返 116; }  // t
    若 (sc == 0x15) { 返 121; }  // y
    若 (sc == 0x16) { 返 117; }  // u
    若 (sc == 0x17) { 返 105; }  // i
    若 (sc == 0x18) { 返 111; }  // o
    若 (sc == 0x19) { 返 112; }  // p
    若 (sc == 0x39) { 返 32; }   // 空格
    若 (sc == 0x1C) { 返 10; }   // 回车
    返 0;
}

空 _start(空) {
    load_font();
    // "甲言内核启动。种子:828"
    put_c(0x80); put_c(0x81); put_c(0x82); put_c(0x83); put_c(0x84);
    put_c(0x85); put_c(0x86); put_c(0x87); put_c(0x88);
    put_s(":828");
    put_nl();
    put_s(">");
    循环 (1) {
        整 s = inb(0x64);
        若 ((s & 1) != 0) {
            整 sc = inb(0x60);
            若 ((sc & 0x80) == 0) {
                整 c = sc_to_ascii(sc);
                若 (c == 10) { put_nl(); put_s(">"); }
                否则 若 (c != 0) { put_c(c); }
            }
        }
    }
}
