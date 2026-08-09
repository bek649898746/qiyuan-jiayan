// v53 — 无 cast 的间接调用: fp = myfn (函数名直接赋值)
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}

整 myfn(整 x){返 x+1;}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v53\n");
    /* 1. 函数名直接赋值 (无 cast) */
    整 (*fp)(整);
    fp=myfn;
    sps(c,"S1\n");
    /* 2. 间接调用 */
    整 r1=fp(41);
    spc(c,'A');spc(c,48+r1);spc(c,10);
    /* 3. 声明时初始化 */
    整 (*fp2)(整)=myfn;
    整 r2=fp2(41);
    spc(c,'B');spc(c,48+r2);spc(c,10);
    若(r1==42&&r2==42){sps(c,"FNPASS\n");}否则{sps(c,"FNFAIL\n");}
    循环(1){__asm(0xF4);}
}
