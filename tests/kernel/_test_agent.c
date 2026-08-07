// 最简 struct 指针测试
typedef struct { 整 x; 整 y; } Pt;

空 serial_init(整 c) {
    outb(c+1,0); outb(c+3,0x80); outb(c+0,1); outb(c+1,0);
    outb(c+3,3); outb(c+2,0xC7); outb(c+4,0x0B);
}
空 serial_wait(整 c) { 循环 ((inb(c+5)&0x20)==0) {} }
空 spc(整 c,整 v) { 若(v==10){serial_wait(c);outb(c,13);} serial_wait(c);outb(c,v); }
空 sps(整 c,字*s) { 循环(*s!=0){spc(c,*s);s++;} }

空 set_pt(Pt *p, 整 v) {
    p->x = v;
    p->y = v + 1;
}

空 _start(空) {
    整 c=0x3F8;
    serial_init(c);
    Pt p;
    set_pt(&p, 5);
    若 (p.x == 5) { sps(c, "X=5\n"); }
    若 (p.y == 6) { sps(c, "Y=6\n"); }
    sps(c, "PTR OK\n");
    循环(1){__asm(0xF4);}
}
