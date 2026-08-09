// v30 — 双写零循环 vs 单写零循环 对比
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v30\n");
    整 a=0x300000;整 b=0x300040;
    sps(c,"A\n");
    整 i=0;
    sps(c,"B\n");
    循环(i<256){*(字节*)(a+i)=0;*(字节*)(b+i)=0;i=i+1;}
    sps(c,"C\n");
    整 j=0;
    循环(j<256){*(字节*)(b+j)=0;j=j+1;}
    sps(c,"D\n");
    sps(c,"a0=");
    整 v=*(整*)a;
    若(v<16)spc(c,48+v);否则 spc(c,87+v);
    spc(c,10);
    sps(c,"DONE\n");
    循环(1){__asm(0xF4);}
}
