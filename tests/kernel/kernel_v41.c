// v41 — 字符串表地址探测: 多个字符串打印规律
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 _start(空){
    整 c=0x3F8;si(c);
    sps(c,"AAA\n");
    sps(c,"BBB\n");
    sps(c,"CCC\n");
    sps(c,"DDD\n");
    sps(c,"EEE\n");
    sps(c,"FFF\n");
    sps(c,"GGG\n");
    sps(c,"HHH\n");
    sps(c,"III\n");
    sps(c,"JJJ\n");
    sps(c,"KKK\n");
    循环(1){__asm(0xF4);}
}
