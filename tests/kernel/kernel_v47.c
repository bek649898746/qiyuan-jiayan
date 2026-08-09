// v47 — 最小函数指针调用 (bin模式)
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hexb(整 c,整 v){spc(c,48+((v>>4)&15));spc(c,48+(v&15));}
空 hex32(整 c,整 v){hexb(c,(v>>28)&15);hexb(c,(v>>24)&15);hexb(c,(v>>20)&15);hexb(c,(v>>16)&15);hexb(c,(v>>12)&15);hexb(c,(v>>8)&15);hexb(c,(v>>4)&15);hexb(c,v&15);}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

整 myfn(整 x){返 x+1;}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v47\n");
    /* 1. 函数地址值 */
    sps(c,"A=");hex32(c,(整)myfn);spc(c,10);
    /* 2. 直接调用 */
    整 r1=myfn(41);
    sps(c,"B=");spn(c,r1);spc(c,10);
    /* 3. 函数指针调用 */
    整 (*fp)(整)=(整 (*)(整))myfn;
    整 r2=fp(41);
    sps(c,"C=");spn(c,r2);spc(c,10);
    /* 4. 判定 */
    若(r2==42){sps(c,"FNPTR-PASS\n");}否则{sps(c,"FNPTR-FAIL\n");}
    循环(1){__asm(0xF4);}
}
