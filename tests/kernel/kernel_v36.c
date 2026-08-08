// v36 — NVMe 数据面终版: 独特CID + 全CQ扫描检测 + I/O队列读写回环
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
/* 槽哨兵完成检测: 在 cq[cmd*4] 写哨兵, 提交后等哨兵变化, 返 DW3 */
整 waitcq(整 cqbase, 整 cmd, 整 cid){
    整 cslot=cmd*4;
    *(整*)(cqbase+cslot*4+0)=0xAAAAAAAA;
    *(整*)(cqbase+cslot*4+1)=0xAAAAAAAA;
    *(整*)(cqbase+cslot*4+2)=0xAAAAAAAA;
    *(整*)(cqbase+cslot*4+3)=0xAAAAAAAA;
    整 t=0;
    循环(t<2000){
        t=t+1;
        若(*(整*)(cqbase+cslot*4+3)!=0xAAAAAAAA){ 返 *(整*)(cqbase+cslot*4+3); }
        若(*(整*)(cqbase+cslot*4+0)!=0xAAAAAAAA){ 返 *(整*)(cqbase+cslot*4+0); }
    }
    返 -1;
}
/* 提交管理命令, 返完成状态 */
整 acmd(整 mmio, 整 asq, 整 acq, 整 cmd, 整 op, 整 cid, 整 nsid, 整 prp1, 整 cdw10, 整 cdw11){
    整 *sq=(整*)asq;
    整 slot=cmd*16;
    sq[slot+0]=op|(cid<<16); sq[slot+1]=nsid; sq[slot+2]=0; sq[slot+3]=0;
    sq[slot+4]=0; sq[slot+5]=0; sq[slot+6]=prp1; sq[slot+7]=0;
    sq[slot+8]=0; sq[slot+9]=0; sq[slot+10]=cdw10; sq[slot+11]=cdw11;
    sq[slot+12]=0; sq[slot+13]=0; sq[slot+14]=0; sq[slot+15]=0;
    *(整*)(mmio+0x1000)=cmd+1;
    返 waitcq(acq, cmd, cid);
}
/* 提交 I/O 命令到 I/O SQ 槽 cmd, 返完成状态 */
整 iocmd(整 mmio, 整 iosq, 整 iocq, 整 cmd, 整 op, 整 cid, 整 prp1, 整 slba, 整 nlb){
    整 *sq=(整*)iosq;
    整 slot=cmd*16;
    sq[slot+0]=op|(cid<<16); sq[slot+1]=1; sq[slot+2]=0; sq[slot+3]=0;
    sq[slot+4]=0; sq[slot+5]=0; sq[slot+6]=prp1; sq[slot+7]=0;
    sq[slot+8]=0; sq[slot+9]=0; sq[slot+10]=slba; sq[slot+11]=0;
    sq[slot+12]=nlb; sq[slot+13]=0; sq[slot+14]=0; sq[slot+15]=0;
    *(整*)(mmio+0x1008)=cmd+1;  /* I/O SQ1 tail @0x1008 */
    整 st=waitcq(iocq, cmd, cid);
    *(整*)(mmio+0x100C)=cmd+1;  /* I/O CQ1 head @0x100C */
    返 st;
}
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v36rw\n");
    整 dev=0, found=0;
    循环(dev < 16 && found==0){
        整 vid=pci32(0,dev,0,0);
        若(vid!=0xFFFFFFFF && vid!=0){
            整 cls=(pci32(0,dev,0,8)>>16)&0xFFFF;
            若(cls==0x0108){
                整 b0=pci32(0,dev,0,0x10);
                整 mmio=b0&~15;
                sps(c,"BAR=");hex32(c,mmio);spc(c,10);
                /* 用 GRUB 曾用的队列地址 (0x7FDD000) + -kernel无GRUB模式 */
                整 aqa=0xFF003F;
                整 asq=0x7FDD000;  /* admin SQ */
                整 acq=0x7FDE000;  /* admin CQ */
                *(整*)(mmio+0x14)=0;
                整 t=0;
                循环((*(整*)(mmio+0x1C))&1){t=t+1;若(t>2000000){sps(c,"T0\n");循环(1){__asm(0xF4);}}}
                sps(c,"R0\n");
                /* 自己的 I/O 队列 + 数据缓冲 */
                整 buf=0x310000;
                整 iocq=0x330000; 整 iosq=0x330800;
                整 i=0;
                循环(i<1024){*(字节*)(acq+i)=0;i=i+1;}
                循环(i<1024+16384){*(字节*)(asq+i-1024)=0;i=i+1;}
                循环(i<4096+17408){*(字节*)(buf+i-17408)=0;i=i+1;}
                循环(i<4096+17408+512){*(字节*)(iocq+i-17408-4096)=0;i=i+1;}
                循环(i<4096+17408+512+2048){*(字节*)(iosq+i-17408-4096-512)=0;i=i+1;}
                sps(c,"ZERO\n");
                *(整*)(mmio+0x24)=aqa;
                *(整*)(mmio+0x28)=asq; *(整*)(mmio+0x2C)=0;
                *(整*)(mmio+0x30)=acq; *(整*)(mmio+0x34)=0;
                sps(c,"QSET\n");
                *(整*)(mmio+0x14)=1|(6<<16)|(4<<20);
                t=0;
                循环((*(整*)(mmio+0x1C))&1==0){t=t+1;若(t>2000000){sps(c,"T2\n");循环(1){__asm(0xF4);}}}
                sps(c,"R1\n");

                整 cmd=0; 整 st;
                sps(c,"GO\n");
                /* Identify Ctl (CNS=1) CID=0x41 — 槽0, tail=1 */
                st=acmd(mmio,asq,acq,cmd,0x06,0x41,0,buf,1,0);
                cmd=cmd+1;
                sps(c,"ID=");hex32(c,*(整*)(buf+4));sps(c,"ST=");hex32(c,st);spc(c,10);
                /* dump 管理 SQ 槽0-2 的 DW0 (确认我的命令在内存) */
                sps(c,"SQd:");
                i=0;
                循环(i<6){
                    hex32(c,*(整*)(asq+i*64));spc(c,32);  /* 每槽 DW0 */
                    i=i+1;
                }
                spc(c,10);
                /* Create I/O CQ (0x05) CID=0x42 — 槽1, tail=2 */
                st=acmd(mmio,asq,acq,cmd,0x05,0x42,0,iocq,1|(31<<16),1);
                cmd=cmd+1;
                sps(c,"CCQ=");hex32(c,st);spc(c,10);
                /* Create I/O SQ (0x01) CID=0x43 — 槽2, tail=3 */
                st=acmd(mmio,asq,acq,cmd,0x01,0x43,0,iosq,1|(31<<16),1|(1<<16));
                cmd=cmd+1;
                sps(c,"CSQ=");hex32(c,st);spc(c,10);
                /* dump 管理 SQ 槽0-3 (create 后确认) */
                sps(c,"SQd:");
                i=0;
                循环(i<4){
                    hex32(c,*(整*)(asq+i*64));spc(c,32);
                    i=i+1;
                }
                spc(c,10);

                /* 写 LBA0 (CID=0x44) */
                i=0;
                循环(i<64){*(字节*)(buf+i)=65+i%26;i=i+1;}
                st=iocmd(mmio,iosq,iocq,0,0x02,0x44,buf,0,0);
                sps(c,"WR=");hex32(c,st);spc(c,10);
                spc(c,'A');spc(c,10);
                hex32(c,*(整*)(iocq+0));spc(c,32);hex32(c,*(整*)(iocq+8));hex32(c,*(整*)(iocq+12));spc(c,10);
                spc(c,'B');spc(c,10);
                hex32(c,*(整*)(iosq+0));hex32(c,*(整*)(iosq+4));spc(c,10);
                spc(c,'C');spc(c,10);
                /* 读 LBA0 (CID=0x45) */
                整 br=buf+0x1000;
                i=0;
                循环(i<64){*(字节*)(br+i)=0;i=i+1;}
                st=iocmd(mmio,iosq,iocq,1,0x01,0x45,br,0,0);
                sps(c,"RD=");hex32(c,st);spc(c,10);
                sps(c,"iCQ2:");hex32(c,*(整*)(iocq+0));hex32(c,*(整*)(iocq+4));hex32(c,*(整*)(iocq+8));hex32(c,*(整*)(iocq+12));spc(c,10);

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
                found=1;
            }
        }
        dev=dev+1;
    }
    若(found==0) sps(c,"NOT FOUND\n");
    循环(1){__asm(0xF4);}
}
