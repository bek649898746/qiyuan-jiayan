// v40 — SQ 槽写入隔离: 直接偏移 vs 指针索引
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hexb(整 c,整 v){spc(c,48+((v>>4)&15));spc(c,48+(v&15));}
空 hex32(整 c,整 v){hexb(c,(v>>28)&15);hexb(c,(v>>24)&15);hexb(c,(v>>20)&15);hexb(c,(v>>16)&15);hexb(c,(v>>12)&15);hexb(c,(v>>8)&15);hexb(c,(v>>4)&15);hexb(c,v&15);}
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v40\n");
    整 base=0x331000;
    /* T1: 直接地址偏移写 */
    *(整*)(base+24)=0x310000;
    sps(c,"T1=");hex32(c,*(整*)(base+24));spc(c,10);
    /* T2: 指针索引写 */
    整 *sq=(整*)base;
    sq[6]=0x310000;
    sps(c,"T2=");hex32(c,*(整*)(base+24));spc(c,10);
    /* T3: 重读 T1 位置 (是否被 T2 覆盖) */
    sps(c,"T3=");hex32(c,*(整*)(base+24));spc(c,10);
    /* T4: 指针索引写不同值 */
    sq[10]=0x12345678;
    sps(c,"T4=");hex32(c,*(整*)(base+40));spc(c,10);
    /* T5: 直接写 DW0 */
    *(整*)base=0x00440002;
    sps(c,"T5=");hex32(c,*(整*)base);spc(c,10);
    循环(1){__asm(0xF4);}
}
