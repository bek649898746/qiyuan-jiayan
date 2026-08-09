// v40 — 基于v38二分: 字符串/字符常量/数字 调用
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hex(整 c,整 v){若(v<10)spc(c,48+v);否则 spc(c,87+v);}
空 hex8(整 c,整 v){hex(c,(v>>4)&15);hex(c,v&15);}
空 hex32(整 c,整 v){hex8(c,(v>>24)&255);hex8(c,(v>>16)&255);hex8(c,(v>>8)&255);hex8(c,v&255);}
空 _start(空){
    整 c=0x3F8;si(c);
    sps(c,"S1\n");
    spc(c,65);spc(c,10);   /* 数字 65 = 'A' */
    spc(c,'B');spc(c,10);  /* 字符 'B' */
    sps(c,"S2\n");
    hex32(c,0x99);spc(c,10);
    sps(c,"S3\n");
    循环(1){__asm(0xF4);}
}
