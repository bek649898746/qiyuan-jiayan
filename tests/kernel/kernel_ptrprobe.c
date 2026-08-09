// 字节指针解引用探针: 声明指针 vs 强转解引用
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hex32(整 c,整 v){spc(c,48+((v>>28)&15));spc(c,48+((v>>24)&15));}
空 _start(空){
    整 c=0x3F8;si(c);
    字 buf[64];
    整 i=0;
    循环(i<64){buf[i]=65+i%26;i=i+1;}
    /* A: 声明指针解引用 */
    字 *p=buf;
    sps(c,"A=");spc(c,*p);spc(c,10);
    /* B: 强转解引用 */
    sps(c,"B=");spc(c,*(字*)(buf+1));spc(c,10);
    /* C: 声明指针 +1 */
    p=p+1;
    sps(c,"C=");spc(c,*p);spc(c,10);
    /* D: 循环遍历 */
    sps(c,"D=");
    p=buf;
    i=0;
    循环(i<16){spc(c,*p);p=p+1;i=i+1;}
    spc(c,10);
    循环(1){__asm(0xF4);}
}
