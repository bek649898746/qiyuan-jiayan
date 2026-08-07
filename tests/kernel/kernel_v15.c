// v15d — 宽扫描找 NVMe (class=0x0108)
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

整 pci32(整 bus,整 dev,整 func,整 off){
    整 a=(1<<31)|(bus<<16)|(dev<<11)|(func<<8)|(off&0xFC);
    outl(0xCF8,a); 返 inl(0xCFC);
}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v15d NVMe SCAN\n");

    整 bus=0, dev, func=0, found=0;
    dev=0;
    循环(dev < 32 && found==0){
        整 vid = pci32(bus, dev, func, 0);
        若(vid != 0xFFFFFFFF && vid != 0){
            整 cc = pci32(bus, dev, func, 0x08);
            整 cls = (cc >> 16) & 0xFFFF;
            若(cls == 0x0108){ // NVMe
                sps(c,"NVMe@");spn(c,dev);spc(c,10);
                整 bar0 = pci32(bus, dev, func, 0x10);
                若((bar0 & 7) == 4){
                    sps(c,"BAR0=64bit ");
                    整 mmio = bar0 & ~15;
                    // 读写 CC 寄存器
                    *(整*)(mmio + 0x14) = 0;
                    *(整*)(mmio + 0x14) = 1;
                    sps(c,"CC.EN=1\n");
                    整 *csts = (整*)(mmio + 0x1C);
                    若(*csts & 1){ sps(c,"RDY=1\n"); }
                }
                found = 1;
            }
        }
        dev = dev + 1;
    }

    若(found) sps(c,"NVMe OK\n");
    否则 sps(c,"NVMe NOT FOUND\n");
    循环(1){__asm(0xF4);}
}
