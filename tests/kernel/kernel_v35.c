// v35 — NVMe 数据面 v2: 零全局变量 (bin模式全局有bug), 状态全走栈参数
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hex(整 c,整 v){若(v<10)spc(c,48+v);否则 spc(c,87+v);}
空 hex8(整 c,整 v){hex(c,(v>>4)&15);hex(c,v&15);}
空 hex32(整 c,整 v){hex8(c,(v>>24)&255);hex8(c,(v>>16)&255);hex8(c,(v>>8)&255);hex8(c,v&255);}
整 pci32(整 bus,整 dev,整 func,整 off){
    整 a=(1<<31)|(bus<<16)|(dev<<11)|(func<<8)|(off&0xFC);
    outl(0xCF8,a); 返 inl(0xCFC);
}
/* 提交管理命令 (全参数, 无全局) — 返回新 cmd 计数 */
整 acmd(整 mmio, 整 asq, 整 acq, 整 cmd, 整 op, 整 cid, 整 nsid, 整 prp1, 整 cdw10, 整 cdw11){
    整 *sq=(整*)asq;
    整 slot=cmd*16;
    sq[slot+0]=op|(cid<<16); sq[slot+1]=nsid; sq[slot+2]=0; sq[slot+3]=0;
    sq[slot+4]=0; sq[slot+5]=0; sq[slot+6]=prp1; sq[slot+7]=0;
    sq[slot+8]=0; sq[slot+9]=0; sq[slot+10]=cdw10; sq[slot+11]=cdw11;
    sq[slot+12]=0; sq[slot+13]=0; sq[slot+14]=0; sq[slot+15]=0;
    整 *cq=(整*)acq;
    整 cslot=cmd*4;
    cq[cslot+0]=0xAAAAAAAA; cq[cslot+1]=0xAAAAAAAA; cq[cslot+2]=0xAAAAAAAA; cq[cslot+3]=0xAAAAAAAA;
    *(整*)(mmio+0x1000)=cmd+1;
    整 t=0;
    循环(t<20000){t=t+1;若(cq[cslot+3]!=0xAAAAAAAA || cq[cslot+0]!=0xAAAAAAAA){break;}}
    返 cmd+1;
}
/* 提交 I/O 命令到 I/O SQ (槽0), 检测 I/O CQ (槽0) */
空 iocmd(整 mmio, 整 iosq, 整 iocq, 整 op, 整 cid, 整 prp1, 整 slba, 整 nlb){
    整 *sq=(整*)iosq;
    sq[0]=op|(cid<<16); sq[1]=1; sq[2]=0; sq[3]=0;
    sq[4]=0; sq[5]=0; sq[6]=prp1; sq[7]=0;
    sq[8]=0; sq[9]=0; sq[10]=slba; sq[11]=0;
    sq[12]=nlb; sq[13]=0; sq[14]=0; sq[15]=0;
    整 *cq=(整*)iocq;
    cq[0]=0xAAAAAAAA; cq[1]=0xAAAAAAAA; cq[2]=0xAAAAAAAA; cq[3]=0xAAAAAAAA;
    *(整*)(mmio+0x1008)=1;  /* I/O SQ1 tail */
    整 t=0;
    循环(t<20000){t=t+1;若(cq[3]!=0xAAAAAAAA || cq[0]!=0xAAAAAAAA){break;}}
    *(整*)(mmio+0x100C)=1;  /* I/O CQ1 head */
}
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v35io\n");
    整 dev=0, found=0;
    循环(dev < 16 && found==0){
        整 vid=pci32(0,dev,0,0);
        若(vid!=0xFFFFFFFF && vid!=0){
            整 cls=(pci32(0,dev,0,8)>>16)&0xFFFF;
            若(cls==0x0108){
                整 b0=pci32(0,dev,0,0x10);
                整 mmio=b0&~15;
                sps(c,"BAR=");hex32(c,mmio);spc(c,10);
                /* 复用 GRUB 预配队列 (0x7FDD000 高地址 + 大队列 — QEMU 才启用) */
                整 aqa=*(整*)(mmio+0x24);
                整 asq=*(整*)(mmio+0x28);
                整 acq=*(整*)(mmio+0x30);
                若(aqa==0){aqa=0xFF003F;}
                若(asq==0){asq=0x7FDD000;}
                若(acq==0){acq=0x7FDE000;}
                *(整*)(mmio+0x14)=0;
                整 t=0;
                循环((*(整*)(mmio+0x1C))&1){t=t+1;若(t>2000000){循环(1){__asm(0xF4);}}}
                sps(c,"R0\n");
                整 buf=0x310000;
                整 iocq=0x330000; 整 iosq=0x330800;  /* 均 4KB 对齐 */
                整 i=0;
                循环(i<512){*(字节*)(acq+i)=0;i=i+1;}
                循环(i<512+1024){*(字节*)(asq+i-512)=0;i=i+1;}
                循环(i<4096+1536){*(字节*)(buf+i-1536)=0;i=i+1;}
                循环(i<4096+1536+512){*(字节*)(iocq+i-1536-4096)=0;i=i+1;}
                循环(i<4096+1536+512+2048){*(字节*)(iosq+i-1536-4096-512)=0;i=i+1;}
                *(整*)(mmio+0x24)=aqa;
                *(整*)(mmio+0x28)=asq; *(整*)(mmio+0x2C)=0;
                *(整*)(mmio+0x30)=acq; *(整*)(mmio+0x34)=0;
                *(整*)(mmio+0x14)=1|(6<<16)|(4<<20);
                t=0;
                循环((*(整*)(mmio+0x1C))&1==0){t=t+1;若(t>2000000){循环(1){__asm(0xF4);}}}
                sps(c,"R1\n");

                整 cmd=0;
                /* Identify Controller (CNS=1) */
                cmd=acmd(mmio,asq,acq,cmd,0x06,0x10,0,buf,1,0);
                sps(c,"ID ");hex32(c,*(整*)(buf+4));spc(c,10);
                /* Create I/O CQ (0x05): QID=1 QSIZE=31 PC=1 */
                cmd=acmd(mmio,asq,acq,cmd,0x05,0x11,0,iocq,1|(31<<16),1);
                sps(c,"CQCQ ");hex32(c,*(整*)(acq+16));hex32(c,*(整*)(acq+24));hex32(c,*(整*)(acq+28));spc(c,10);
                /* Create I/O SQ (0x01): QID=1 QSIZE=31 PC=1 CQID=1 */
                cmd=acmd(mmio,asq,acq,cmd,0x01,0x12,0,iosq,1|(31<<16),1|(1<<16));
                sps(c,"CQSQ ");hex32(c,*(整*)(acq+32));hex32(c,*(整*)(acq+40));hex32(c,*(整*)(acq+44));spc(c,10);

                /* 写 LBA0 */
                i=0;
                循环(i<64){*(字节*)(buf+i)=65+i%26;i=i+1;}
                sps(c,"WR\n");
                iocmd(mmio,iosq,iocq,0x02,0x22,buf,0,0);
                sps(c,"WRCQ ");hex32(c,*(整*)(iocq+0));hex32(c,*(整*)(iocq+8));hex32(c,*(整*)(iocq+12));spc(c,10);
                /* 读 LBA0 */
                整 br=buf+0x1000;
                i=0;
                循环(i<64){*(字节*)(br+i)=0;i=i+1;}
                sps(c,"RD\n");
                iocmd(mmio,iosq,iocq,0x01,0x33,br,0,0);
                sps(c,"RDCQ ");hex32(c,*(整*)(iocq+0));hex32(c,*(整*)(iocq+8));hex32(c,*(整*)(iocq+12));spc(c,10);
                /* 回环 */
                整 ok=1;
                i=0;
                循环(i<64){
                    若(*(字节*)(buf+i)!=*(字节*)(br+i)){ok=0;}
                    i=i+1;
                }
                sps(c,"W=");
                字节 *wb=(字节*)buf;
                i=0;循环(i<16){spc(c,*wb);wb=wb+1;i=i+1;}
                sps(c,"R=");
                字节 *rb=(字节*)br;
                i=0;循环(i<16){spc(c,*rb);rb=rb+1;i=i+1;}
                spc(c,10);
                若(ok==1){sps(c,"LOOPBACK-PASS\n");}否则{sps(c,"LOOPBACK-FAIL\n");}
                sps(c,"CSTS=");hex32(c,*(整*)(mmio+0x1C));spc(c,10);
                found=1;
            }
        }
        dev=dev+1;
    }
    若(found==0) sps(c,"NOT FOUND\n");
    循环(1){__asm(0xF4);}
}
