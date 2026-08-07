// v16h — 用局部指针 (match 成功的 v15f 模式)
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hex8(整 c,整 v){spc(c,((v>>4)&15)<10?48+((v>>4)&15):87+((v>>4)&15));spc(c,(v&15)<10?48+(v&15):87+(v&15));}

整 pci32(整 bus,整 dev,整 func,整 off){
    整 a=(1<<31)|(bus<<16)|(dev<<11)|(func<<8)|(off&0xFC);
    outl(0xCF8,a); 返 inl(0xCFC);
}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v16h\n");

    整 dev=0; 循环(dev<16){
        整 vid=pci32(0,dev,0,0);
        若(vid!=0xFFFFFFFF&&vid!=0){
            整 cc=pci32(0,dev,0,8);
            若(((cc>>16)&0xFFFF)==0x0108){
                整 bar=pci32(0,dev,0,0x10)&~15;

                // 用局部指针变量 (match 成功的模式)
                字节 *p = (字节*)bar;
                sps(c,"CAP0=");hex8(c,p[0]);spc(c,10);

                整 *cc_reg = (整*)(bar+0x14);
                sps(c,"CC=");hex8(c,cc_reg[0]>>8);hex8(c,cc_reg[0]);spc(c,10);

                sps(c,"WR CC=0...\n");
                cc_reg[0] = 0;
                sps(c,"WR OK\n");
                break;
            }
        }
        dev=dev+1;
    }
    循环(1){__asm(0xF4);}
}
