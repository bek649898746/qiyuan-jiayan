// v33b — NVMe 读写回环 v2: 命令计数器 (每命令独立槽位 + tail)
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
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v33b\n");
    整 dev=0, found=0;
    循环(dev < 16 && found==0){
        整 vid=pci32(0,dev,0,0);
        若(vid!=0xFFFFFFFF && vid!=0){
            整 cls=(pci32(0,dev,0,8)>>16)&0xFFFF;
            若(cls==0x0108){
                整 b0=pci32(0,dev,0,0x10);
                整 mmio=b0&~15;
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
                整 i=0;
                循环(i<256){*(字节*)(asq+i)=0;i=i+1;}
                循环(i<512){*(字节*)(acq+i-256)=0;i=i+1;}
                循环(i<8192){*(字节*)(buf+i-512)=0;i=i+1;}
                *(整*)(mmio+0x24)=aqa;
                *(整*)(mmio+0x28)=asq; *(整*)(mmio+0x2C)=0;
                *(整*)(mmio+0x30)=acq; *(整*)(mmio+0x34)=0;
                *(整*)(mmio+0x14)=1|(6<<16)|(4<<20);
                t=0;
                循环((*(整*)(mmio+0x1C))&1==0){t=t+1;若(t>2000000){循环(1){__asm(0xF4);}}}
                sps(c,"R1\n");

                整 *sq=(整*)asq;
                整 *cq=(整*)acq;
                整 cmd=0;  /* 命令计数器: 槽位 + tail */
                /* 辅助: 提交命令到槽 cmd, tail=cmd+1, 哨兵检测槽 cmd 完成 */
                /* cmd=0: Identify Namespace (NSID=1, CNS=0) */
                sq[cmd*16+0]=0x06|(0x11<<16); sq[cmd*16+1]=1; sq[cmd*16+2]=0; sq[cmd*16+3]=0;
                sq[cmd*16+4]=0; sq[cmd*16+5]=0; sq[cmd*16+6]=buf; sq[cmd*16+7]=0;
                sq[cmd*16+8]=0; sq[cmd*16+9]=0; sq[cmd*16+10]=0; sq[cmd*16+11]=0;
                sq[cmd*16+12]=0; sq[cmd*16+13]=0; sq[cmd*16+14]=0; sq[cmd*16+15]=0;
                cq[cmd*4+0]=0xAAAAAAAA; cq[cmd*4+1]=0xAAAAAAAA; cq[cmd*4+2]=0xAAAAAAAA; cq[cmd*4+3]=0xAAAAAAAA;
                *(整*)(mmio+0x1000)=cmd+1;  /* SQ tail */
                t=0;
                循环(t<20000){t=t+1;若(cq[cmd*4+3]!=0xAAAAAAAA || cq[cmd*4+0]!=0xAAAAAAAA){break;}}
                sps(c,"NS ");hex32(c,cq[cmd*4+0]);hex32(c,cq[cmd*4+2]);hex32(c,cq[cmd*4+3]);spc(c,10);
                /* dump 关键字段: FLBAS@0x12, LBAF0@0x40, NSZE@0x00 */
                sps(c,"NSD:");
                i=0;
                循环(i<6){hex8(c,*(字节*)(buf+i));spc(c,32);i=i+1;}
                sps(c,"FLBAS=");hex8(c,*(字节*)(buf+0x12));spc(c,10);
                sps(c,"LBAF0=");
                i=0;
                循环(i<4){hex8(c,*(字节*)(buf+0x40+i));spc(c,32);i=i+1;}
                spc(c,10);
                整 nsize0=*(整*)(buf+0);
                整 nsize1=*(整*)(buf+4);
                整 flbas=*(字节*)(buf+0x12);  /* FLBAS: 活动格式索引 */
                整 lbads=*(字节*)(buf+0x40+(flbas&15)*4)&15;
                整 lbsz=1;
                i=0;循环(i<lbads){lbsz=lbsz*2;i=i+1;}
                sps(c,"NSZE=");hex32(c,nsize1);hex32(c,nsize0);
                sps(c,"LBSZ=");hex32(c,lbsz);spc(c,10);
                若(lbsz<512){lbsz=512;}
                cmd=cmd+1;

                /* cmd=1: 写 LBA0 (数据=buf 前512B 模式) */
                i=0;
                循环(i<64){*(字节*)(buf+i)=65+i%26;i=i+1;}
                sq[cmd*16+0]=0x02|(0x22<<16); sq[cmd*16+1]=1; sq[cmd*16+2]=0; sq[cmd*16+3]=0;
                sq[cmd*16+4]=0; sq[cmd*16+5]=0; sq[cmd*16+6]=buf; sq[cmd*16+7]=0;
                sq[cmd*16+8]=0; sq[cmd*16+9]=0; sq[cmd*16+10]=0; sq[cmd*16+11]=0;
                sq[cmd*16+12]=0; sq[cmd*16+13]=0; sq[cmd*16+14]=0; sq[cmd*16+15]=0;
                cq[cmd*4+0]=0xAAAAAAAA; cq[cmd*4+1]=0xAAAAAAAA; cq[cmd*4+2]=0xAAAAAAAA; cq[cmd*4+3]=0xAAAAAAAA;
                *(整*)(mmio+0x1000)=cmd+1;
                t=0;
                循环(t<20000){t=t+1;若(cq[cmd*4+3]!=0xAAAAAAAA || cq[cmd*4+0]!=0xAAAAAAAA){break;}}
                sps(c,"WR ");hex32(c,cq[cmd*4+0]);hex32(c,cq[cmd*4+2]);hex32(c,cq[cmd*4+3]);spc(c,10);
                cmd=cmd+1;

                /* cmd=2: 读 LBA0 (数据→buf+0x1000) */
                整 br=buf+0x1000;
                i=0;
                循环(i<64){*(字节*)(br+i)=0;i=i+1;}
                sq[cmd*16+0]=0x01|(0x33<<16); sq[cmd*16+1]=1; sq[cmd*16+2]=0; sq[cmd*16+3]=0;
                sq[cmd*16+4]=0; sq[cmd*16+5]=0; sq[cmd*16+6]=br; sq[cmd*16+7]=0;
                sq[cmd*16+8]=0; sq[cmd*16+9]=0; sq[cmd*16+10]=0; sq[cmd*16+11]=0;
                sq[cmd*16+12]=0; sq[cmd*16+13]=0; sq[cmd*16+14]=0; sq[cmd*16+15]=0;
                cq[cmd*4+0]=0xAAAAAAAA; cq[cmd*4+1]=0xAAAAAAAA; cq[cmd*4+2]=0xAAAAAAAA; cq[cmd*4+3]=0xAAAAAAAA;
                *(整*)(mmio+0x1000)=cmd+1;
                t=0;
                循环(t<20000){t=t+1;若(cq[cmd*4+3]!=0xAAAAAAAA || cq[cmd*4+0]!=0xAAAAAAAA){break;}}
                sps(c,"RD ");hex32(c,cq[cmd*4+0]);hex32(c,cq[cmd*4+2]);hex32(c,cq[cmd*4+3]);spc(c,10);

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
