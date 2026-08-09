// v38 — 内存写探针: 各地址 int/字节写落地测试 (不碰 NVMe)
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hex32(整 c,整 v){spc(c,48+((v>>28)&15));spc(c,48+((v>>24)&15));}
空 hexb(整 c,整 v){spc(c,48+((v>>4)&15));spc(c,48+(v&15));}
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v38\n");
    整 addrs[5]; addrs[0]=0x310000; addrs[1]=0x330000; addrs[2]=0x331000; addrs[3]=0x7FDD000; addrs[4]=0x300000;
    整 ai=0;
    循环(ai<5){
        整 a=addrs[ai];
        *(整*)a=0x11223344;                 /* int 写 */
        *(整*)(a+4)=0x55667788;
        *(字节*)(a+8)=0x41; *(字节*)(a+9)=0x42;  /* 字节写 */
        sps(c,"A");hex32(c,a);sps(c,":");
        hex32(c,*(整*)a);spc(c,44);hex32(c,*(整*)(a+4));spc(c,44);
        hexb(c,*(字节*)(a+8));hexb(c,*(字节*)(a+9));spc(c,10);
        ai=ai+1;
    }
    循环(1){__asm(0xF4);}
}
