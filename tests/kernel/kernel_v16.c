// v16m — 直接 *(整*)0xFEBD4000 vs 通过变量
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hex8(整 c,整 v){spc(c,((v>>4)&15)<10?48+((v>>4)&15):87+((v>>4)&15));spc(c,(v&15)<10?48+(v&15):87+(v&15));}

整 pci32(整 b,整 d,整 f,整 o){整 a=(1<<31)|(b<<16)|(d<<11)|(f<<8)|(o&0xFC);outl(0xCF8,a);返 inl(0xCFC);}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v16m\n");
    整 d=0;循环(d<16){整 v=pci32(0,d,0,0);若(v!=0xFFFFFFFF&&v!=0){整 c2=pci32(0,d,0,8);若(((c2>>16)&0xFFFF)==0x0108){整 bar=pci32(0,d,0,0x10)&~15;
    sps(c,"bar ok\n");

    // 方法A: 直接 *(整*)bar (不通过局部变量)
    整 a = *(整*)bar;
    sps(c,"A=");hex8(c,a>>8);hex8(c,a);spc(c,10);

    // 方法B: 通过字节*
    字节 *bp = (字节*)bar;
    整 b = bp[0];
    sps(c,"B=");hex8(c,b);spc(c,10);

    break;
    }}d=d+1;}
    循环(1){__asm(0xF4);}
}
