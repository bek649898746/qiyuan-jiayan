// v22 — Get Features (无DMA) 隔离命令路径
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
    整 c=0x3F8;si(c);sps(c,"v22\n");
    整 dev=0, found=0;
    循环(dev < 16 && found==0){
        整 vid=pci32(0,dev,0,0);
        若(vid!=0xFFFFFFFF && vid!=0){
            整 cls=(pci32(0,dev,0,8)>>16)&0xFFFF;
            若(cls==0x0108){
                整 b0=pci32(0,dev,0,0x10);
                整 mmio=b0&~15;
                sps(c,"BAR=");hex32(c,mmio);spc(c,10);
                *(整*)(mmio+0x14)=0;
                整 t=0;
                循环((*(整*)(mmio+0x1C))&1){t=t+1;若(t>2000000){sps(c,"T1");循环(1){__asm(0xF4);}}}
                sps(c,"RDY0\n");
                整 acq=0x300000; 整 asq=0x300040;
                整 i=0;
                循环(i<256){*(字节*)(acq+i)=0;*(字节*)(asq+i)=0;i=i+1;}
                *(整*)(mmio+0x20)=0x10001;
                *(整*)(mmio+0x24)=asq; *(整*)(mmio+0x28)=0;
                *(整*)(mmio+0x2C)=acq; *(整*)(mmio+0x30)=0;
                *(整*)(mmio+0x14)=1|(6<<16)|(4<<20);
                t=0;
                循环((*(整*)(mmio+0x1C))&1==0){t=t+1;若(t>2000000){sps(c,"T2");循环(1){__asm(0xF4);}}}
                sps(c,"RDY1\n");

                /* Get Features FID=1 (Number of Queues), opcode 0x0A, 无 DMA */
                整 *sq=(整*)asq;
                sq[0]=0x0A; sq[1]=0; sq[2]=0; sq[3]=0;
                sq[4]=0; sq[5]=0; sq[6]=0; sq[7]=0;
                sq[8]=0; sq[9]=0; sq[10]=1; sq[11]=0;
                sq[12]=0; sq[13]=0; sq[14]=0; sq[15]=0;
                *(整*)(mmio+0x1000)=1;
                整 *cq=(整*)acq;
                t=0;
                循环(((cq[2]>>16)&1)==0){t=t+1;若(t>2000000){sps(c,"CQTO");break;}}
                sps(c,"CQ=");
                hex32(c,cq[0]);hex32(c,cq[1]);hex32(c,cq[2]);hex32(c,cq[3]);
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
