// v20n — NVMe 真驱动: PCIe枚举 → BAR0 MMIO → 控制器启用 → Admin队列 → Identify
// 依赖 2026-08-08 修复: *(整*)MMIO 4字节读写 (pesz), 字节类型映射
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}
空 hex(整 c,整 v){若(v<10)spc(c,48+v);否则 spc(c,87+v);}
空 hex8(整 c,整 v){hex(c,(v>>4)&15);hex(c,v&15);}
空 hex32(整 c,整 v){hex8(c,(v>>24)&255);hex8(c,(v>>16)&255);hex8(c,(v>>8)&255);hex8(c,v&255);}

整 pci32(整 bus,整 dev,整 func,整 off){
    整 a=(1<<31)|(bus<<16)|(dev<<11)|(func<<8)|(off&0xFC);
    outl(0xCF8,a); 返 inl(0xCFC);
}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v20n NVMe\n");
    整 dev=0, found=0;

    循环(dev < 16 && found==0){
        整 vid=pci32(0,dev,0,0);
        若(vid!=0xFFFFFFFF && vid!=0){
            整 cc=pci32(0,dev,0,8);
            整 cls=(cc>>16)&0xFFFF;
            若(cls==0x0108){
                sps(c,"dev=");spn(c,dev);spc(c,10);
                整 b0=pci32(0,dev,0,0x10);
                整 b1=pci32(0,dev,0,0x14);
                整 mmio=b0&~15;
                sps(c,"BAR=");hex32(c,b1);hex32(c,b0);spc(c,10);

                /* CAP0 验证 (64位读拆两个32位) */
                整 cap_lo=*(整*)mmio;
                整 cap_hi=*(整*)(mmio+4);
                sps(c,"CAP=");hex32(c,cap_hi);hex32(c,cap_lo);spc(c,10);
                sps(c,"VS=");hex32(c,*(整*)(mmio+8));spc(c,10);

                /* 1. 禁用控制器 (CC.EN=0) */
                *(整*)(mmio+0x14)=0;
                /* 等待 CSTS.RDY==0 */
                整 t=0;
                循环((*(整*)(mmio+0x1C))&1){
                    t=t+1;若(t>2000000){sps(c,"T1");循环(1){__asm(0xF4);}}
                }
                sps(c,"RDY0\n");

                /* 2. 固定物理地址队列 (identity 映射, <4GB) */
                整 acq=0x300000;  /* Admin CQ: 2 entry * 16B */
                整 asq=0x300040;  /* Admin SQ: 2 entry * 64B */
                整 buf=0x310000;  /* Identify 数据缓冲 4KB 对齐 */
                /* 清零队列与缓冲 (字节写) */
                整 i=0;
                循环(i<256){*(字节*)(acq+i)=0;*(字节*)(asq+i)=0;i=i+1;}
                循环(i<4096+256){*(字节*)(buf+i-256)=0;i=i+1;}

                /* 3. AQA @0x24: ACQS=1 ASQS=1 (各2 entry) — 注意 NVMe 寄存器偏移: 0x20=NSSR, 0x24=AQA, 0x28=ASQ(64), 0x30=ACQ(64) */
                *(整*)(mmio+0x24)=0x10001;
                /* 4. ASQ/ACQ 基址 (32位写入, 高32=0) */
                *(整*)(mmio+0x28)=asq;
                *(整*)(mmio+0x2C)=0;
                *(整*)(mmio+0x30)=acq;
                *(整*)(mmio+0x34)=0;
                /* 5. CC: EN=1 IOSQES=6 IOCQES=4 MPS=0 */
                *(整*)(mmio+0x14)=1|(6<<16)|(4<<20);
                /* 6. 等待 CSTS.RDY==1 */
                t=0;
                循环((*(整*)(mmio+0x1C))&1==0){
                    t=t+1;若(t>2000000){sps(c,"T2");循环(1){__asm(0xF4);}}
                }
                sps(c,"RDY1\n");

                /* 7. Identify 命令 (opcode 0x06, CNS=1 controller) */
                整 *sq=(整*)asq;
                sq[0]=0x06;      /* opcode */
                sq[1]=0;         /* NSID=0 (controller) */
                sq[2]=0;
                sq[3]=0;
                sq[4]=0;         /* MPTR */
                sq[5]=0;
                sq[6]=buf;       /* PRP1 = 4KB 对齐缓冲 */
                sq[7]=0;
                sq[8]=0;         /* PRP2 */
                sq[9]=0;
                sq[10]=1;        /* CDW10: CNS=1 */
                sq[11]=0;
                sq[12]=0;
                sq[13]=0;
                sq[14]=0;
                sq[15]=0;
                /* 提交: tail=1 */
                *(整*)(mmio+0x1000)=1;
                /* 8. 等待 CQ 完成 (phase bit: DW2 bit16) */
                整 *cq=(整*)acq;
                t=0;
                循环(((cq[2]>>16)&1)==0){
                    t=t+1;若(t>2000000){sps(c,"CQTO");循环(1){__asm(0xF4);}}
                }
                /* 更新 CQ head */
                *(整*)(mmio+0x1004)=1;
                sps(c,"ID OK\n");

                /* 9. 读 Identify 数据 */
                sps(c,"SN=");
                字节 *sn=(字节*)(buf+4);  /* SN: bytes 4-23 */
                i=0;
                循环(i<20){spc(c,*sn);sn=sn+1;i=i+1;}
                spc(c,10);
                sps(c,"NN=");
                整 nn=*(整*)(buf+0x10);   /* NN: bytes 16-19 */
                spn(c,nn);spc(c,10);
                sps(c,"MN=");
                字节 *mn=(字节*)(buf+0x24); /* MN: bytes 36-75 */
                i=0;
                循环(i<40){spc(c,*mn);mn=mn+1;i=i+1;}
                spc(c,10);

                found=1;
            }
        }
        dev=dev+1;
    }
    若(found==0) sps(c,"NOT FOUND\n");
    循环(1){__asm(0xF4);}
}
