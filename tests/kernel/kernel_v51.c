// v51 — 间接调用逐步隔离
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}

整 myfn(整 x){返 x+1;}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v51\n");
    /* 1. 无初始化声明 */
    整 (*fp)(整);
    sps(c,"S1\n");
    /* 2. 赋值 */
    fp=(整 (*)(整))myfn;
    sps(c,"S2\n");
    /* 3. 调用 */
    整 r2=fp(41);
    spc(c,'A');spc(c,48+r2);spc(c,10);
    /* 4. 字面量调用 (无变量) */
    整 r3=((整 (*)(整))(整)myfn)(41);
    spc(c,'B');spc(c,48+r3);spc(c,10);
    /* 5. 内存槽 */
    *(整*)0x320000=(整)myfn;
    整 r4=((整 (*)(整))(*(整*)0x320000))(41);
    spc(c,'C');spc(c,48+r4);spc(c,10);
    若(r2==42&&r3==42&&r4==42){sps(c,"FNPASS\n");}否则{sps(c,"FNFAIL\n");}
    循环(1){__asm(0xF4);}
}
