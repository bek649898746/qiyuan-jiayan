// v38 — 连续调用序列隔离测试 (纯串口, 无 NVMe)
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hex(整 c,整 v){若(v<10)spc(c,48+v);否则 spc(c,87+v);}
空 hex8(整 c,整 v){hex(c,(v>>4)&15);hex(c,v&15);}
空 hex32(整 c,整 v){hex8(c,(v>>24)&255);hex8(c,(v>>16)&255);hex8(c,(v>>8)&255);hex8(c,v&255);}
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"START\n");
    /* 模拟 WR= 模式 */
    sps(c,"W=");hex32(c,0x11223344);spc(c,10);
    /* 连续调用序列 */
    spc(c,'A');spc(c,10);
    hex32(c,0xAABBCCDD);spc(c,10);
    spc(c,'B');spc(c,10);
    hex32(c,0x55667788);spc(c,10);
    spc(c,'C');spc(c,10);
    sps(c,"END\n");
    循环(1){__asm(0xF4);}
}
