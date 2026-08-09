// v15f — 扫描找 NVMe, hex BAR0, 谨慎读 MMIO
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

整 pci32(整 bus,整 dev,整 func,整 off){
    整 a=(1<<31)|(bus<<16)|(dev<<11)|(func<<8)|(off&0xFC);
    outl(0xCF8,a); 返 inl(0xCFC);
}

空 hex(整 c,整 v){ 若(v<10)spc(c,48+v);否则 spc(c,87+v); }
空 hex8(整 c,整 v){ hex(c,(v>>4)&15); hex(c,v&15); }

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v15f SCAN\n");
    整 dev=0, found=0;

    循环(dev < 16 && found==0){
        整 vid=pci32(0,dev,0,0);
        若(vid!=0xFFFFFFFF && vid!=0){
            整 cc=pci32(0,dev,0,8);
            整 cls=(cc>>16)&0xFFFF;
            若(cls==0x0108){
                sps(c,"NVMe dev=");spn(c,dev);spc(c,10);
                整 b0=pci32(0,dev,0,0x10);
                整 b1=pci32(0,dev,0,0x14);

                sps(c,"BAR=");
                hex8(c,(b1>>24)&255);hex8(c,(b1>>16)&255);
                hex8(c,(b1>>8)&255);hex8(c,b1&255);
                hex8(c,(b0>>24)&255);hex8(c,(b0>>16)&255);
                hex8(c,(b0>>8)&255);hex8(c,b0&255);
                spc(c,10);

                若((b0&7)==4){ // 64-bit MMIO
                    整 mmio=b0&~15;
                    sps(c,"try RD MMIO+0...\n");
                    字节 *p=(字节*)mmio;
                    字节 bb=*p;
                    sps(c,"CAP0=");hex8(c,bb);spc(c,10);
                    sps(c,"MMIO OK!\n");
                }
                found=1;
            }
        }
        dev=dev+1;
    }
    若(found==0) sps(c,"NOT FOUND\n");
    循环(1){__asm(0xF4);}
}
