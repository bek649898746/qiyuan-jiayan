// v29 — GRUB队列复用 + 快速轮询诊断 CQ
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
    整 c=0x3F8;si(c);sps(c,"v29\n");
    整 dev=0, found=0;
    循环(dev < 16 && found==0){
        整 vid=pci32(0,dev,0,0);
        若(vid!=0xFFFFFFFF && vid!=0){
            整 cls=(pci32(0,dev,0,8)>>16)&0xFFFF;
            若(cls==0x0108){
                整 b0=pci32(0,dev,0,0x10);
                整 mmio=b0&~15;
                整 asq=*(整*)(mmio+0x28);
                整 acq=*(整*)(mmio+0x30);
                sps(c,"ASQ=");hex32(c,asq);sps(c,"ACQ=");hex32(c,acq);spc(c,10);
                整 buf=0x310000;
                整 i=0;
                循环(i<4096){*(字节*)(buf+i)=0;i=i+1;}
                整 *sq=(整*)asq;
                sq[0]=0x06; sq[1]=0; sq[2]=0; sq[3]=0;
                sq[4]=0; sq[5]=0; sq[6]=buf; sq[7]=0;
                sq[8]=0; sq[9]=0; sq[10]=1; sq[11]=0;
                sq[12]=0; sq[13]=0; sq[14]=0; sq[15]=0;
                sps(c,"SQW0\n");
                *(整*)(mmio+0x1000)=1;
                sps(c,"DBL\n");
                整 *cq=(整*)acq;
                sps(c,"CQb=");
                hex32(c,cq[0]);hex32(c,cq[1]);hex32(c,cq[2]);hex32(c,cq[3]);
                spc(c,10);
                /* 快速轮询: 每 2000 次打一次状态, 上限 20000 */
                整 t=0;
                循环(t<20000){
                    t=t+1;
                    若(((cq[2]>>16)&1)==1){sps(c,"DONE ");hex32(c,t);spc(c,10);break;}
                    若((t%2000)==0){
                        sps(c,"T");hex32(c,t);sps(c,"CQ=");
                        hex32(c,cq[0]);hex32(c,cq[1]);hex32(c,cq[2]);hex32(c,cq[3]);
                        spc(c,10);
                    }
                }
                sps(c,"CSTS=");hex32(c,*(整*)(mmio+0x1C));spc(c,10);
                sps(c,"SN=");
                字节 *sn=(字节*)(buf+4);
                i=0;
                循环(i<20){spc(c,*sn);sn=sn+1;i=i+1;}
                spc(c,10);
                found=1;
            }
        }
        dev=dev+1;
    }
    若(found==0) sps(c,"NOT FOUND\n");
    循环(1){__asm(0xF4);}
}
