// v37 — 数据面调试: 逐步打印 buf 状态 (填充后/写后/读后)
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hexb(整 c,整 v){spc(c,48+((v>>4)&15));spc(c,48+(v&15));}
空 hex32(整 c,整 v){hexb(c,(v>>28)&15);hexb(c,(v>>24)&15);hexb(c,(v>>20)&15);hexb(c,(v>>16)&15);hexb(c,(v>>12)&15);hexb(c,(v>>8)&15);hexb(c,(v>>4)&15);hexb(c,v&15);}
空 dump(整 c,整 *base,整 n){整 i=0;循环(i<n){hexb(c,*(字节*)(base+i));spc(c,44);i=i+1;}spc(c,10);}
整 pci32(整 bus,整 dev,整 func,整 off){
    整 a=(1<<31)|(bus<<16)|(dev<<11)|(func<<8)|(off&0xFC);
    outl(0xCF8,a); 返 inl(0xCFC);
}
整 waitcq(整 cqbase, 整 entries, 整 cid){
    整 t=0;
    循环(t<20000){
        整 i=0;
        循环(i<entries){
            整 dw2=*(整*)(cqbase+i*16+8);
            若((dw2&0xFFFF)==cid){ 返 *(整*)(cqbase+i*16+12); }
            i=i+1;
        }
        t=t+1;
    }
    返 -1;
}
整 acmd(整 mmio, 整 asq, 整 acq, 整 cmd, 整 op, 整 cid, 整 nsid, 整 prp1, 整 cdw10, 整 cdw11){
    整 *sq=(整*)asq;
    整 slot=cmd*16;
    sq[slot+0]=op|(cid<<16); sq[slot+1]=nsid; sq[slot+2]=0; sq[slot+3]=0;
    sq[slot+4]=0; sq[slot+5]=0; sq[slot+6]=prp1; sq[slot+7]=0;
    sq[slot+8]=0; sq[slot+9]=0; sq[slot+10]=cdw10; sq[slot+11]=cdw11;
    sq[slot+12]=0; sq[slot+13]=0; sq[slot+14]=0; sq[slot+15]=0;
    *(整*)(mmio+0x1000)=cmd+1;
    返 waitcq(acq, 64, cmd+1);
}
整 iocmd(整 mmio, 整 iosq, 整 iocq, 整 cmd, 整 op, 整 cid, 整 prp1, 整 slba, 整 nlb){
    整 *sq=(整*)iosq;
    整 slot=cmd*16;
    sq[slot+0]=op|(cid<<16); sq[slot+1]=1; sq[slot+2]=0; sq[slot+3]=0;
    sq[slot+4]=0; sq[slot+5]=0; sq[slot+6]=prp1; sq[slot+7]=0;
    sq[slot+8]=0; sq[slot+9]=0; sq[slot+10]=slba; sq[slot+11]=0;
    sq[slot+12]=nlb; sq[slot+13]=0; sq[slot+14]=0; sq[slot+15]=0;
    *(整*)(mmio+0x1008)=cmd+1;
    整 st=waitcq(iocq, 32, cmd+1);
    *(整*)(mmio+0x100C)=cmd+1;
    返 st;
}
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v37\n");
    整 dev=0, found=0;
    循环(dev < 16 && found==0){
        整 vid=pci32(0,dev,0,0);
        若(vid!=0xFFFFFFFF && vid!=0){
            整 cls=(pci32(0,dev,0,8)>>16)&0xFFFF;
            若(cls==0x0108){
                整 b0=pci32(0,dev,0,0x10);
                整 mmio=b0&~15;
                outl(0xCF8,(1<<31)|(dev<<11)|(0x10&0xFC));
                outl(0xCFC,0xFFFFFFFF);
                outl(0xCF8,(1<<31)|(dev<<11)|(0x10&0xFC));
                outl(0xCFC,b0);
                sps(c,"BAR=");hex32(c,mmio);spc(c,10);
                整 aqa=0xFF003F;
                整 asq=0x7FDD000; 整 acq=0x7FDE000;
                *(整*)(mmio+0x14)=0;
                整 t=0;
                循环((*(整*)(mmio+0x1C))&1){t=t+1;若(t>2000000){sps(c,"T0\n");循环(1){__asm(0xF4);}}}
                sps(c,"R0\n");
                整 buf=0x310000; 整 iocq=0x330000; 整 iosq=0x331000;
                整 i=0;
                循环(i<1024){*(字节*)(acq+i)=0;i=i+1;}
                循环(i<17408){*(字节*)(asq+i-1024)=0;i=i+1;}
                循环(i<21504){*(字节*)(buf+i-17408)=0;i=i+1;}
                循环(i<22016){*(字节*)(iocq+i-21504)=0;i=i+1;}
                循环(i<24064){*(字节*)(iosq+i-22016)=0;i=i+1;}
                *(整*)(mmio+0x24)=aqa;
                *(整*)(mmio+0x28)=asq; *(整*)(mmio+0x2C)=0;
                *(整*)(mmio+0x30)=acq; *(整*)(mmio+0x34)=0;
                sps(c,"QSET\n");
                整 rtry=0; 整 rdy1=0;
                循环(rtry<5 && rdy1==0){
                    *(整*)(mmio+0x14)=1|(6<<16)|(4<<20);
                    t=0;
                    循环((*(整*)(mmio+0x1C))&1==0){t=t+1;若(t>3000000){break;}}
                    若((*(整*)(mmio+0x1C))&1){rdy1=1;}
                    否则{*(整*)(mmio+0x14)=0;t=0;循环((*(整*)(mmio+0x1C))&1){t=t+1;若(t>1000000){break;}}}
                    rtry=rtry+1;
                }
                若(rdy1==0){sps(c,"RDYFAIL\n");循环(1){__asm(0xF4);}}
                sps(c,"R1\n");
                整 cmd=0; 整 st;
                st=acmd(mmio,asq,acq,cmd,0x06,0x41,0,buf,1,0); cmd=cmd+1;
                sps(c,"ID=");hex32(c,*(整*)(buf+4));sps(c,"ST=");hex32(c,st);spc(c,10);
                st=acmd(mmio,asq,acq,cmd,0x05,0x42,0,iocq,1|(31<<16),1); cmd=cmd+1;
                sps(c,"CCQ=");hex32(c,st);spc(c,10);
                st=acmd(mmio,asq,acq,cmd,0x01,0x43,0,iosq,1|(31<<16),1|(1<<16)); cmd=cmd+1;
                sps(c,"CSQ=");hex32(c,st);spc(c,10);
                /* 填充 buf 并打印 (写前状态) */
                i=0;
                循环(i<64){*(字节*)(buf+i)=65+i%26;i=i+1;}
                sps(c,"P1=");dump(c,buf,16);
                /* 手动填 I/O SQ 槽0 并 dump 验证命令落地 */
                整 *sq=(整*)iosq;
                sq[0]=0x02|(0x44<<16); sq[1]=1; sq[6]=buf; sq[10]=0; sq[12]=0;
                sps(c,"SQ0=");hex32(c,*(整*)(iosq+0));spc(c,44);hex32(c,*(整*)(0x331018));spc(c,10);  /* DW0 + DW6(字节偏移24) */
                sps(c,"PA=");dump(c,buf,16);  /* doorbell 前 buf */
                *(整*)(mmio+0x1008)=1;  /* I/O SQ1 tail=1 */
                sps(c,"PB=");dump(c,buf,16);  /* doorbell 后立即 */
                st=waitcq(iocq, 32, 1);
                *(整*)(mmio+0x100C)=1;
                sps(c,"WR=");hex32(c,st);spc(c,10);
                sps(c,"PC=");dump(c,buf,16);  /* 完成后 buf */
                /* 读 LBA0 到 br */
                整 br=buf+0x1000;
                循环(i<64){*(字节*)(br+i)=0;i=i+1;}
                st=iocmd(mmio,iosq,iocq,1,0x01,0x45,br,0,0);
                sps(c,"RD=");hex32(c,st);spc(c,10);
                sps(c,"P3=");dump(c,br,16);   /* 读回内容 */
                sps(c,"P4=");dump(c,buf,16);  /* 读后 buf */
                /* 真回环: 期望 buf 保持 A-Z */
                整 ok=1;
                i=0;
                循环(i<16){若(*(字节*)(buf+i)!=65+i%26){ok=0;}i=i+1;}
                若(ok==1){sps(c,"BUF-OK\n");}否则{sps(c,"BUF-LOST\n");}
                found=1;
            }
        }
        dev=dev+1;
    }
    若(found==0) sps(c,"NOT FOUND\n");
    循环(1){__asm(0xF4);}
}
