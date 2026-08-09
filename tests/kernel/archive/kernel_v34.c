// v34 — NVMe 完整数据面: Create I/O CQ/SQ → 读写回环
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
/* 全局: 管理队列 + I/O 队列状态 */
整 g_mmio; 整 g_asq; 整 g_acq; 整 g_iocq; 整 g_iosq; 整 g_cmd=0; 整 g_cqbase;
/* 提交管理命令到槽 g_cmd, 哨兵检测 */
空 acmd(整 op, 整 cid, 整 nsid, 整 prp1, 整 cdw10, 整 cdw11){
    整 *sq=(整*)g_asq;
    整 slot=g_cmd*16;
    sq[slot+0]=op|(cid<<16); sq[slot+1]=nsid; sq[slot+2]=0; sq[slot+3]=0;
    sq[slot+4]=0; sq[slot+5]=0; sq[slot+6]=prp1; sq[slot+7]=0;
    sq[slot+8]=0; sq[slot+9]=0; sq[slot+10]=cdw10; sq[slot+11]=cdw11;
    sq[slot+12]=0; sq[slot+13]=0; sq[slot+14]=0; sq[slot+15]=0;
    整 *cq=(整*)g_acq;
    整 cslot=g_cmd*4;
    cq[cslot+0]=0xAAAAAAAA; cq[cslot+1]=0xAAAAAAAA; cq[cslot+2]=0xAAAAAAAA; cq[cslot+3]=0xAAAAAAAA;
    *(整*)(g_mmio+0x1000)=g_cmd+1;  /* admin SQ tail */
    整 t=0;
    循环(t<20000){
        t=t+1;
        若(cq[cslot+3]!=0xAAAAAAAA || cq[cslot+0]!=0xAAAAAAAA){break;}
    }
    g_cmd=g_cmd+1;
}
/* 提交 I/O 命令到 I/O SQ, 哨兵检测 I/O CQ */
空 iocmd(整 op, 整 cid, 整 prp1, 整 slba, 整 nlb){
    整 *sq=(整*)g_iosq;
    整 slot=0;  /* 单命令, 槽0 */
    sq[slot+0]=op|(cid<<16); sq[slot+1]=1; sq[slot+2]=0; sq[slot+3]=0;
    sq[slot+4]=0; sq[slot+5]=0; sq[slot+6]=prp1; sq[slot+7]=0;
    sq[slot+8]=0; sq[slot+9]=0; sq[slot+10]=slba; sq[slot+11]=0;
    sq[slot+12]=nlb; sq[slot+13]=0; sq[slot+14]=0; sq[slot+15]=0;
    整 *cq=(整*)g_iocq;
    cq[0]=0xAAAAAAAA; cq[1]=0xAAAAAAAA; cq[2]=0xAAAAAAAA; cq[3]=0xAAAAAAAA;
    *(整*)(g_mmio+0x1008)=1;  /* I/O SQ1 tail doorbell @0x1008 */
    整 t=0;
    循环(t<20000){
        t=t+1;
        若(cq[3]!=0xAAAAAAAA || cq[0]!=0xAAAAAAAA){break;}
    }
    *(整*)(g_mmio+0x100C)=1;  /* I/O CQ1 head doorbell @0x100C */
}
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v34io\n");
    整 dev=0, found=0;
    循环(dev < 16 && found==0){
        整 vid=pci32(0,dev,0,0);
        若(vid!=0xFFFFFFFF && vid!=0){
            整 cls=(pci32(0,dev,0,8)>>16)&0xFFFF;
            若(cls==0x0108){
                整 b0=pci32(0,dev,0,0x10);
                g_mmio=b0&~15;
                sps(c,"BAR=");hex32(c,g_mmio);spc(c,10);
                整 aqa=*(整*)(g_mmio+0x24);
                整 asq=*(整*)(g_mmio+0x28);
                整 acq=*(整*)(g_mmio+0x30);
                若(aqa==0){aqa=0xFF003F;}
                若(asq==0){asq=0x7FDD000;}
                若(acq==0){acq=0x7FDE000;}
                g_asq=asq; g_acq=acq;
                *(整*)(g_mmio+0x14)=0;
                整 t=0;
                循环((*(整*)(g_mmio+0x1C))&1){t=t+1;若(t>2000000){循环(1){__asm(0xF4);}}}
                sps(c,"R0\n");
                /* 缓冲布局: 0x310000 数据, 0x330000 I/O CQ, 0x330200 I/O SQ */
                整 buf=0x310000;
                g_iocq=0x330000; g_iosq=0x330200;
                整 i=0;
                循环(i<256){*(字节*)(asq+i)=0;i=i+1;}
                循环(i<512){*(字节*)(acq+i-256)=0;i=i+1;}
                循环(i<4096){*(字节*)(buf+i-512)=0;i=i+1;}
                循环(i<4096+512){*(字节*)(g_iocq+i-512)=0;i=i+1;}
                循环(i<4096+512+512){*(字节*)(g_iosq+i-1024)=0;i=i+1;}
                *(整*)(g_mmio+0x24)=aqa;
                *(整*)(g_mmio+0x28)=asq; *(整*)(g_mmio+0x2C)=0;
                *(整*)(g_mmio+0x30)=acq; *(整*)(g_mmio+0x34)=0;
                *(整*)(g_mmio+0x14)=1|(6<<16)|(4<<20);
                t=0;
                循环((*(整*)(g_mmio+0x1C))&1==0){t=t+1;若(t>2000000){循环(1){__asm(0xF4);}}}
                sps(c,"R1\n");

                /* Identify Controller 确认 */
                acmd(0x06, 0x10, 0, buf, 1, 0);
                sps(c,"ID ");hex32(c,*(整*)(buf+4));spc(c,10);

                /* Create I/O CQ (0x05): QID=1 QSIZE=31(32条目) PC=1 */
                acmd(0x05, 0x11, 0, g_iocq, 1|(31<<16), 1);
                sps(c,"CQCQ\n");
                /* Create I/O SQ (0x01): QID=1 QSIZE=31 PC=1 CQID=1 */
                acmd(0x01, 0x12, 0, g_iosq, 1|(31<<16), 1|(1<<16));
                sps(c,"CQSQ\n");

                /* 写 LBA0: 数据=buf 模式 */
                i=0;
                循环(i<64){*(字节*)(buf+i)=65+i%26;i=i+1;}
                sps(c,"WR\n");
                iocmd(0x02, 0x22, buf, 0, 0);
                sps(c,"WRCQ ");hex32(c,*(整*)(g_iocq+0));hex32(c,*(整*)(g_iocq+8));hex32(c,*(整*)(g_iocq+12));spc(c,10);

                /* 读 LBA0 → buf+0x1000 */
                整 br=buf+0x1000;
                i=0;
                循环(i<64){*(字节*)(br+i)=0;i=i+1;}
                sps(c,"RD\n");
                iocmd(0x01, 0x33, br, 0, 0);
                sps(c,"RDCQ ");hex32(c,*(整*)(g_iocq+0));hex32(c,*(整*)(g_iocq+8));hex32(c,*(整*)(g_iocq+12));spc(c,10);

                /* 回环校验 */
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
                sps(c,"CSTS=");hex32(c,*(整*)(g_mmio+0x1C));spc(c,10);
                found=1;
            }
        }
        dev=dev+1;
    }
    若(found==0) sps(c,"NOT FOUND\n");
    循环(1){__asm(0xF4);}
}
