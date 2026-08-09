// v19 — 二分定位 bin 模式崩溃触发构造
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hex8(整 c,整 v){spc(c,((v>>4)&15)<10?48+((v>>4)&15):87+((v>>4)&15));spc(c,(v&15)<10?48+(v&15):87+(v&15));}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v19\n");
    整 t=0xB8000;整 i=0;
    *(整*)t=0x11223344;
    sps(c,"W1\n");
    整 a=*(字节*)t; sps(c,"R1\n");
    整 b=*(字节*)(t+0); sps(c,"R2\n");
    整 d=*(字节*)(t+i); sps(c,"R3\n");
    整 e=*(字节*)(t+i+0); sps(c,"R4\n");
    hex8(c,a);hex8(c,b);hex8(c,d);hex8(c,e);spc(c,10);
    循环(1){__asm(0xF4);}
}
