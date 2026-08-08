// v32 — NVMe 正式驱动: 复用 GRUB 队列配置 + Identify 读回 SN/NN/MN
// 2026-08-08 突破: 队列配置需大队列(AQA=0xFF003F) + 高地址(0x7FDD000)
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
    整 c=0x3F8;si(c);sps(c,"v32 NVMe\n");
    整 dev=0, found=0;
    循环(dev < 16 && found==0){
        整 vid=pci32(0,dev,0,0);
        若(vid!=0xFFFFFFFF && vid!=0){
            整 cls=(pci32(0,dev,0,8)>>16)&0xFFFF;
            若(cls==0x0108){
                整 b0=pci32(0,dev,0,0x10);
                整 mmio=b0&~15;
                sps(c,"BAR=");hex32(c,mmio);spc(c,10);
                /* 读 GRUB/BIOS 预配的队列配置 (大队列+高地址是 QEMU 需要的) */
                整 aqa=*(整*)(mmio+0x24);
                整 asq=*(整*)(mmio+0x28);
                整 acq=*(整*)(mmio+0x30);
                若(aqa==0){aqa=0xFF003F;}  /* 兜底: 64 CQ / 256 SQ */
                若(asq==0){asq=0x7FDD000;}
                若(acq==0){acq=0x7FDE000;}
                sps(c,"AQA=");hex32(c,aqa);spc(c,10);
                sps(c,"ASQ=");hex32(c,asq);spc(c,10);
                sps(c,"ACQ=");hex32(c,acq);spc(c,10);

                /* 1. 禁用控制器 */
                *(整*)(mmio+0x14)=0;
                整 t=0;
                循环((*(整*)(mmio+0x1C))&1){t=t+1;若(t>2000000){循环(1){__asm(0xF4);}}}
                sps(c,"R0\n");
                /* 2. 清零队列 + Identify 缓冲 */
                整 buf=0x310000;
                整 i=0;
                循环(i<256){*(字节*)(asq+i)=0;i=i+1;}
                循环(i<512){*(字节*)(acq+i-256)=0;i=i+1;}
                循环(i<4096+512){*(字节*)(buf+i-512)=0;i=i+1;}
                /* 3. 写队列配置 */
                *(整*)(mmio+0x24)=aqa;
                *(整*)(mmio+0x28)=asq; *(整*)(mmio+0x2C)=0;
                *(整*)(mmio+0x30)=acq; *(整*)(mmio+0x34)=0;
                /* 4. 启用 */
                *(整*)(mmio+0x14)=1|(6<<16)|(4<<20);
                t=0;
                循环((*(整*)(mmio+0x1C))&1==0){t=t+1;若(t>2000000){循环(1){__asm(0xF4);}}}
                sps(c,"R1\n");

                /* 5. Identify (CNS=1 controller) */
                整 *sq=(整*)asq;
                sq[0]=0x06; sq[1]=0; sq[2]=0; sq[3]=0;
                sq[4]=0; sq[5]=0; sq[6]=buf; sq[7]=0;
                sq[8]=0; sq[9]=0; sq[10]=1; sq[11]=0;
                sq[12]=0; sq[13]=0; sq[14]=0; sq[15]=0;
                *(整*)(mmio+0x1000)=1;  /* SQTDBL */
                /* 6. 等完成 (轮询 CQ 数据变化) */
                整 *cq=(整*)acq;
                t=0;
                循环(t<20000){
                    t=t+1;
                    若(cq[3]!=0){break;}  /* 完成状态非零 = 已写入 */
                    若(cq[0]!=0){break;}
                    若(cq[2]!=0){break;}
                }
                /* 7. 读 Identify 数据 */
                sps(c,"SN=");
                字节 *sn=(字节*)(buf+4);
                i=0;
                循环(i<20){spc(c,*sn);sn=sn+1;i=i+1;}
                spc(c,10);
                sps(c,"NN=");
                整 nn=*(整*)(buf+0x100);  /* NN @ 0x100 */
                hex32(c,nn);spc(c,10);
                sps(c,"MN=");
                字节 *mn=(字节*)(buf+0x18);  /* MN @ 0x18 */
                i=0;
                循环(i<40){spc(c,*mn);mn=mn+1;i=i+1;}
                spc(c,10);
                sps(c,"CSTS=");hex32(c,*(整*)(mmio+0x1C));spc(c,10);
                found=1;
            }
        }
        dev=dev+1;
    }
    若(found==0) sps(c,"NOT FOUND\n");
    循环(1){__asm(0xF4);}
}
