// v43 — Tensor 池 NVMe 持久化: 版本链数据块落盘 (QEMU 可验证)
// 布局: 索引表@RAM 0x2000000 (64×52), 暂存块@索引后, 磁盘池@LBA 0x200 (每槽 8 LBA = 4KB)
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}
空 hexb(整 c,整 v){spc(c,48+((v>>4)&15));spc(c,48+(v&15));}

#define TENTRIES 64
#define TBLOCK 4096
#define TPOOL_BASE 0x2000000
#define DISK_BASE_LBA 0x200

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
/* I/O 读写: op=0x01 写 / 0x02 读, slba 起始LBA, nlb 块数-1.
   io_n = 累计 I/O 命令数 (0起), 槽位=(io_n&31), tail/head=(io_n+1)&31, 完成CID=槽位+1 */
整 iorw(整 mmio, 整 iosq, 整 iocq, 整 io_n, 整 op, 整 cid, 整 prp1, 整 slba, 整 nlb){
    整 *sq=(整*)iosq;
    整 slot=io_n&31;
    sq[slot*16+0]=op|(cid<<16); sq[slot*16+1]=1; sq[slot*16+2]=0; sq[slot*16+3]=0;
    sq[slot*16+4]=0; sq[slot*16+5]=0; sq[slot*16+6]=prp1; sq[slot*16+7]=0;
    sq[slot*16+8]=0; sq[slot*16+9]=0; sq[slot*16+10]=slba; sq[slot*16+11]=0;
    sq[slot*16+12]=nlb; sq[slot*16+13]=0; sq[slot*16+14]=0; sq[slot*16+15]=0;
    *(整*)(mmio+0x1008)=((io_n+1)&31);  /* I/O SQ1 tail */
    整 st=waitcq(iocq, 32, slot+1);      /* QEMU: 完成 CID=槽位+1 */
    *(整*)(mmio+0x100C)=((io_n+1)&31);  /* I/O CQ1 head */
    返 st;
}

/* ---- Tensor 池 (v37 版本链, 数据块映射到磁盘 LBA) ---- */
整 tphash(字 *key){
    整 h=5381;
    循环(*key){h=((h<<5)+h)+*key;key=key+1;}
    返 h&63;
}
整 tp_find(整 tbl, 字 *key, 整 wantmax, 整 wantv){
    整 best=-1; 整 bestv=-1;
    整 i=0;
    循环(i<TENTRIES){
        整 fl=*(整*)(tbl+i*52+48);
        若(fl==1){
            字 *nm=(字*)(tbl+i*52);
            整 match=1; 整 j=0;
            循环(key[j]&&j<31){若(nm[j]!=key[j]){match=0;break;}j=j+1;}
            若(match&&nm[j]==0){
                整 v=*(整*)(tbl+i*52+40);
                若(wantmax){若(v>bestv){bestv=v;best=i;}}
                否则{若(v==wantv){返 i;}}
            }
        }
        i=i+1;
    }
    返 wantmax?best:-1;
}
/* 存: 分配槽, 数据块=磁盘 LBA(DISK_BASE_LBA+i*8), 返 RAM 暂存地址 (0=表满) */
整 tpool_put(整 tbl, 字 *key, 整 size){
    整 idx=tphash(key);
    整 probe=0;
    循环(probe<TENTRIES){
        整 i=(idx+probe)&63;
        整 fl=*(整*)(tbl+i*52+48);
        若(fl==0){
            整 j=0; 循环(key[j]&&j<31){*(字节*)(tbl+i*52+j)=key[j];j=j+1;}
            *(字节*)(tbl+i*52+j)=0;
            整 v=1; 整 prev=-1;
            整 old=tp_find(tbl,key,1,0);
            若(old>=0){v=*(整*)(tbl+old*52+40)+1;prev=old;}
            *(整*)(tbl+i*52+32)=DISK_BASE_LBA+i*8;  /* 磁盘 LBA */
            *(整*)(tbl+i*52+36)=size;
            *(整*)(tbl+i*52+40)=v;
            *(整*)(tbl+i*52+44)=prev;
            *(整*)(tbl+i*52+48)=1;
            返 TPOOL_BASE+TENTRIES*52+i*TBLOCK;  /* RAM 暂存 */
        }
        probe=probe+1;
    }
    返 0;
}
/* 提交: 把 key 最新版本暂存块写入磁盘 (PRP1=暂存RAM地址, slba=磁盘LBA) */
整 tpool_commit(整 mmio, 整 iosq, 整 iocq, 整 io_n, 整 tbl, 字 *key){
    整 i=tp_find(tbl,key,1,0);
    若(i<0)返 -1;
    整 lba=*(整*)(tbl+i*52+32);
    整 staging=TPOOL_BASE+TENTRIES*52+i*TBLOCK;
    返 iorw(mmio,iosq,iocq,io_n,0x01,0x50+i,staging,lba,1);
}
/* 取最新版本: 读磁盘到 out */
整 tpool_get(整 mmio, 整 iosq, 整 iocq, 整 io_n, 整 tbl, 字 *key, 整 out){
    整 i=tp_find(tbl,key,1,0);
    若(i<0)返 0;
    整 lba=*(整*)(tbl+i*52+32);
    整 st=iorw(mmio,iosq,iocq,io_n,0x02,0x60+i,out,lba,1);
    返 st==-1?0:out;
}
整 tpool_version(整 tbl, 字 *key){
    整 i=tp_find(tbl,key,1,0);
    若(i>=0)返 *(整*)(tbl+i*52+40);
    返 0;
}
整 tpool_del(整 tbl, 字 *key){
    整 i=0; 整 found=0;
    循环(i<TENTRIES){
        整 fl=*(整*)(tbl+i*52+48);
        若(fl==1){
            字 *nm=(字*)(tbl+i*52);
            整 match=1; 整 j=0;
            循环(key[j]&&j<31){若(nm[j]!=key[j]){match=0;break;}j=j+1;}
            若(match&&nm[j]==0){*(整*)(tbl+i*52+48)=0;found=1;}
        }
        i=i+1;
    }
    返 found?0:-1;
}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v43\n");
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
                整 aqa=0xFF003F;
                整 asq=0x7FDD000; 整 acq=0x7FDE000;
                *(整*)(mmio+0x14)=0;
                整 t=0;
                循环((*(整*)(mmio+0x1C))&1){t=t+1;若(t>2000000){sps(c,"T0\n");循环(1){__asm(0xF4);}}}
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
                整 cmd=0; 整 st;
                st=acmd(mmio,asq,acq,cmd,0x06,0x41,0,buf,1,0); cmd=cmd+1;
                sps(c,"ID=");hexb(c,*(字节*)(buf+4));hexb(c,*(字节*)(buf+5));hexb(c,*(字节*)(buf+6));hexb(c,*(字节*)(buf+7));sps(c,"ST=");hexb(c,(st>>8)&15);hexb(c,(st>>4)&15);spc(c,10);
                st=acmd(mmio,asq,acq,cmd,0x05,0x42,0,iocq,1|(31<<16),1); cmd=cmd+1;
                sps(c,"CCQ=");hexb(c,(st>>8)&15);hexb(c,(st>>4)&15);spc(c,10);
                st=acmd(mmio,asq,acq,cmd,0x01,0x43,0,iosq,1|(31<<16),1|(1<<16)); cmd=cmd+1;
                sps(c,"CSQ=");hexb(c,(st>>8)&15);hexb(c,(st>>4)&15);spc(c,10);

                /* Tensor 池初始化 */
                整 tbl=TPOOL_BASE;
                i=0; 循环(i<TENTRIES*52){*(字节*)(tbl+i)=0;i=i+1;}
                /* 清零磁盘池区 (写 0x200 起 64 块) */
                整 pbuf=0x320000;
                i=0;循环(i<4096){*(字节*)(pbuf+i)=0;i=i+1;}
                整 io_n=0;
                st=iorw(mmio,iosq,iocq,io_n,0x01,0x70,pbuf,DISK_BASE_LBA,63); io_n=io_n+1;
                sps(c,"ZP=");hexb(c,(st>>8)&15);hexb(c,(st>>4)&15);spc(c,10);

                /* 测试1: hello v1 "ABCDE" */
                整 a=tpool_put(tbl,"hello",5);
                若(a==0){sps(c,"PUT1-FAIL\n");循环(1){__asm(0xF4);}}
                i=0;循环(i<5){*(字节*)(a+i)=65+i;i=i+1;}
                st=tpool_commit(mmio,iosq,iocq,io_n,tbl,"hello"); io_n=io_n+1;
                sps(c,"C1=");hexb(c,(st>>8)&15);hexb(c,(st>>4)&15);spc(c,10);
                sps(c,"V1=");spn(c,tpool_version(tbl,"hello"));spc(c,10);
                /* 测试2: hello v2 "KLMNO" */
                整 b=tpool_put(tbl,"hello",5);
                i=0;循环(i<5){*(字节*)(b+i)=75+i;i=i+1;}
                st=tpool_commit(mmio,iosq,iocq,io_n,tbl,"hello"); io_n=io_n+1;
                sps(c,"C2=");hexb(c,(st>>8)&15);hexb(c,(st>>4)&15);spc(c,10);
                sps(c,"V2=");spn(c,tpool_version(tbl,"hello"));spc(c,10);
                /* 测试3: 取最新 → 磁盘读回 "KLMNO" */
                整 out=0x320000;
                i=0;循环(i<4096){*(字节*)(out+i)=0;i=i+1;}
                st=tpool_get(mmio,iosq,iocq,io_n,tbl,"hello",out); io_n=io_n+1;
                sps(c,"G=");
                i=0;循环(i<5){spc(c,*(字节*)(out+i));i=i+1;}
                spc(c,10);
                /* 测试4: 校验 v1 磁盘块 (slot A 的 LBA) — 通过索引取 v1 版本槽 */
                整 i1=tp_find(tbl,"hello",0,1);
                若(i1>=0){
                    整 lba1=*(整*)(tbl+i1*52+32);
                    i=0;循环(i<4096){*(字节*)(out+i)=0;i=i+1;}
                    st=iorw(mmio,iosq,iocq,io_n,0x02,0x80+i1,out,lba1,1); io_n=io_n+1;
                    sps(c,"G1=");
                    i=0;循环(i<5){spc(c,*(字节*)(out+i));i=i+1;}
                    spc(c,10);
                }否则{sps(c,"G1=NF\n");}
                /* 判定: 重新读最新版本检查 (out 已被 G1 覆盖) */
                整 ok=1;
                若(tpool_version(tbl,"hello")!=2){ok=0;}
                i=0;循环(i<4096){*(字节*)(out+i)=0;i=i+1;}
                st=tpool_get(mmio,iosq,iocq,io_n,tbl,"hello",out); io_n=io_n+1;
                若(*(字节*)(out+0)!=75){ok=0;}   /* 最新 K */
                若(ok==1){sps(c,"PERSIST-PASS\n");}否则{sps(c,"PERSIST-FAIL\n");}
                found=1;
            }
        }
        dev=dev+1;
    }
    若(found==0) sps(c,"NOT FOUND\n");
    循环(1){__asm(0xF4);}
}
