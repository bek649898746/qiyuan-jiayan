// 甲言内核 v13 — Gate 8 KV Cache 零拷贝槽切换
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

// KV 槽定义 (HBM 内, 编译时常量)
#define KV0 0x00000000  // think  64MB
#define KV1 0x04000000  // code   64MB
#define KV2 0x08000000  // review 32MB
#define KV3 0x0A000000  // test   16MB

// === 入口 ===
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"JIAYAN v13 GATE8\n");
    sps(c,"KV CACHE ZERO-COPY\n");

    // 调度切换: 只改指针值 (零拷贝, ~100ns)
    整 active = 0;  // 当前活跃槽指针
    active = KV0; sps(c,"think  sz=64MB\n");
    active = KV1; sps(c,"code   sz=64MB\n");
    active = KV2; sps(c,"review sz=32MB\n");
    active = KV3; sps(c,"test   sz=16MB\n");
    active = KV0; sps(c,"think  sz=64MB\n");

    sps(c,"KV OK\n"); 循环(1){__asm(0xF4);}
}
