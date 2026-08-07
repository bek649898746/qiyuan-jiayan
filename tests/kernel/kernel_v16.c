// v16o — 对比 1/2/4 字节 MMIO 读
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hex8(整 c,整 v){spc(c,((v>>4)&15)<10?48+((v>>4)&15):87+((v>>4)&15));spc(c,(v&15)<10?48+(v&15):87+(v&15));}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v16o\n");
    整 val=0xFEBD4000;

    字节 *b=(字节*)val; sps(c,"B=");hex8(c,b[0]);spc(c,10);
    无 短 *s=(无 短*)val; sps(c,"try W...\n"); 整 w=s[0]; sps(c,"W=");hex8(c,w>>8);hex8(c,w);spc(c,10);
    整 *i=(整*)val; sps(c,"try I...\n"); 整 iv=i[0]; sps(c,"I=");hex8(c,iv>>8);hex8(c,iv);spc(c,10);

    循环(1){__asm(0xF4);}
}
