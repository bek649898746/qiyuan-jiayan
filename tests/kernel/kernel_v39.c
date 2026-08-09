// v39 — 直接地址 int/字节写落地测试
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hexb(整 c,整 v){spc(c,48+((v>>4)&15));spc(c,48+(v&15));}
空 hex8(整 c,整 v){hexb(c,(v>>28)&15);hexb(c,(v>>24)&15);hexb(c,(v>>20)&15);hexb(c,(v>>16)&15);hexb(c,(v>>12)&15);hexb(c,(v>>8)&15);hexb(c,(v>>4)&15);hexb(c,v&15);}
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v39\n");
    /* T1: 直接 int 写 0x331000 */
    *(整*)0x331000=0x11223344;
    sps(c,"T1=");hex8(c,*(整*)0x331000);spc(c,10);
    /* T2: 直接 int 写 0x310000 */
    *(整*)0x310000=0xAABBCCDD;
    sps(c,"T2=");hex8(c,*(整*)0x310000);spc(c,10);
    /* T3: 字节写 0x331000 */
    *(字节*)0x331000=0x41;
    sps(c,"T3=");hexb(c,*(字节*)0x331000);spc(c,10);
    /* T4: int 写 0x7FDD000 */
    *(整*)0x7FDD000=0xDEADBEEF;
    sps(c,"T4=");hex8(c,*(整*)0x7FDD000);spc(c,10);
    /* T5: 局部变量 i 递增循环写 */
    整 i=0; 整 base=0x310000;
    循环(i<4){*(整*)(base+i*4)=0x11111111+i;i=i+1;}
    sps(c,"T5=");hex8(c,*(整*)0x310000);spc(c,44);hex8(c,*(整*)0x310004);spc(c,44);hex8(c,*(整*)0x310008);spc(c,44);hex8(c,*(整*)0x31000C);spc(c,10);
    循环(1){__asm(0xF4);}
}
