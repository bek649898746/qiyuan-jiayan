// 修复 spn: 三位数补中间0
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}

// 修复版 spn: 正确处理中间0
空 spn(整 c,整 n){
    若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;}
    若(n>=100){
        spc(c,48+n/100);
        n=n-(n/100)*100;
        若(n<10){spc(c,48);}  // 补十位0!
    }
    若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}
    spc(c,48+n);
}

整 add2(整 a,整 b){ 返 a+b; }
整 retb(整 a,整 b){ 返 b; }

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"FIXED\n");
    spn(c,100);spc(c,10);
    spn(c,99);spc(c,10);
    spn(c,101);spc(c,10);
    spn(c, add2(1,99));spc(c,10);
    spn(c, retb(1,99));spc(c,10);
    循环(1){__asm(0xF4);}
}
