// v50 — 间接调用变体测试: 局部变量 vs 字面量 vs 内存槽
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 hexb(整 c,整 v){spc(c,48+((v>>4)&15));spc(c,48+(v&15));}
空 hex32(整 c,整 v){hexb(c,(v>>28)&15);hexb(c,(v>>24)&15);hexb(c,(v>>20)&15);hexb(c,(v>>16)&15);hexb(c,(v>>12)&15);hexb(c,(v>>8)&15);hexb(c,(v>>4)&15);hexb(c,v&15);}

整 myfn(整 x){返 x+1;}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v50\n");
    /* 1. 函数地址 (应 0x104000+) */
    sps(c,"A=");hex32(c,(整)myfn);spc(c,10);
    /* 2. 直接调用 (对照) */
    整 r1=myfn(41);
    sps(c,"B=");spc(c,48+r1);spc(c,10);
    /* 3. 局部变量间接调用 */
    整 (*fp)(整)=(整 (*)(整))myfn;
    sps(c,"C=");hex32(c,(整)fp);spc(c,10);
    整 r2=fp(41);
    sps(c,"D=");spc(c,48+r2);spc(c,10);
    /* 4. 字面量间接调用 (无局部变量) */
    整 r3=((整 (*)(整))(整)myfn)(41);
    sps(c,"E=");spc(c,48+r3);spc(c,10);
    /* 5. 内存槽间接调用 (模拟工具表) */
    *(整*)0x320000=(整)myfn;
    sps(c,"F=");hex32(c,*(整*)0x320000);spc(c,10);
    整 (*fp2)(整)=(整 (*)(整))(*(整*)0x320000);
    整 r4=fp2(41);
    sps(c,"G=");spc(c,48+r4);spc(c,10);
    /* 判定 */
    若(r2==42 && r3==42 && r4==42){sps(c,"FNPASS\n");}否则{sps(c,"FNFAIL\n");}
    循环(1){__asm(0xF4);}
}
