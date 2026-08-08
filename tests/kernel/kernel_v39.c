// v39 — 调用序列细分: 字符常量 vs 数字 vs 字符串
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hex(整 c,整 v){若(v<10)spc(c,48+v);否则 spc(c,87+v);}
空 hex8(整 c,整 v){hex(c,(v>>4)&15);hex(c,v&15);}
空 hex32(整 c,整 v){hex8(c,(v>>24)&255);hex8(c,(v>>16)&255);hex8(c,(v>>8)&255);hex8(c,v&255);}
空 _start(空){
    整 c=0x3F8;si(c);
    /* T1: 数字常量 */
    spc(c,48);spc(c,49);spc(c,10);
    /* T2: 字符常量 */
    spc(c,'X');spc(c,10);
    /* T3: 数字 hex */
    hex32(c,0x1234);spc(c,10);
    /* T4: 字符 hex */
    hex32(c,'Y');spc(c,10);
    /* T5: 字符串 */
    sps(c,"T5OK\n");
    /* T6: 混合 */
    sps(c,"T6=");hex32(c,0x99);spc(c,10);
    循环(1){__asm(0xF4);}
}
