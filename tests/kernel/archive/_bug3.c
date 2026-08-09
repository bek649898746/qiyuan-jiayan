// Bug #3 诊断: 直接输出 p2.x 值
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

typedef struct { 整 x; 整 y; } Pt;

空 _start(空){
    整 c=0x3F8;si(c);

    Pt p1, p2;
    p1.x = 10; p1.y = 20;
    p2.x = 30; p2.y = 40;

    sps(c,"p1.x=");spn(c,p1.x);spc(c,10);
    sps(c,"p1.y=");spn(c,p1.y);spc(c,10);
    sps(c,"p2.x=");spn(c,p2.x);spc(c,10);
    sps(c,"p2.y=");spn(c,p2.y);spc(c,10);

    循环(1){__asm(0xF4);}
}
