// v16 final — 字节组装32位绕过编译器bug
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

整 pci32(整 b,整 d,整 f,整 o){整 a=(1<<31)|(b<<16)|(d<<11)|(f<<8)|(o&0xFC);outl(0xCF8,a);返 inl(0xCFC);}

// 字节读写32位值 (绕过 *(整*) 编译器bug)
整 rd32(整 bar, 整 off){
    字节 *p=(字节*)bar;
    整 lo = p[off] | (p[off+1]<<8) | (p[off+2]<<16) | (p[off+3]<<24);
    返 lo;
}
空 wr32(整 bar, 整 off, 整 v){
    字节 *p=(字节*)bar;
    p[off]=v&255; p[off+1]=(v>>8)&255; p[off+2]=(v>>16)&255; p[off+3]=(v>>24)&255;
}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"JIAYAN v16 NVMe\n");

    // 扫描NVMe
    整 d=0,bar=0;循环(d<16){整 v=pci32(0,d,0,0);若(v!=0xFFFFFFFF&&v!=0){整 cc=pci32(0,d,0,8);若(((cc>>16)&0xFFFF)==0x0108){bar=pci32(0,d,0,0x10)&~15;break;}}d=d+1;}
    若(bar==0){sps(c,"NOT FOUND\n");循环(1){}}

    // 1. 禁用 (字节写)
    wr32(bar,0x14,0);
    整 t=0;循环((rd32(bar,0x1C)&1)&&t<1000){t=t+1;}

    // 2. 设 AdminQ
    wr32(bar,0x28,0x200000); wr32(bar,0x2C,0);
    wr32(bar,0x30,0x202000); wr32(bar,0x34,0);
    wr32(bar,0x24,0x000F000F);

    // 3. 启用 CC.EN=1
    wr32(bar,0x14,1|(4<<16)|(6<<20));
    t=0;循环(!(rd32(bar,0x1C)&1)&&t<10000){t=t+1;}
    若(!(rd32(bar,0x1C)&1)){sps(c,"RDY TO\n");循环(1){}}
    sps(c,"RDY=1\n");

    // 4. Identify
    整 *sq=(整*)0x200000;
    sq[0]=6; sq[4]=0x203000;
    wr32(bar,0x1000,1);

    // 5. 等 CQ
    t=0; 整 *cq=(整*)0x202000;
    循环((cq[3]&0x8000)==0&&t<100000){t=t+1;}
    若(cq[3]&0x8000){
        字节 *id=(字节*)0x203000;
        sps(c,"MODEL=");
        整 i=24;循环(i<32){字节 b=id[i];若(b>31&&b<127)spc(c,b);i=i+1;}
        spc(c,10);
    }否则{sps(c,"CQ TO\n");}

    sps(c,"NVMe OK\n");循环(1){__asm(0xF4);}
}
