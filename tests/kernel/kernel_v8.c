// 甲言内核 v8 — Gate 4 调度器 FINAL (2参数绕过 r8 bug)
// Bug: 函数第3+参数(r8/r9)传值错误 → 预算≤255 用2参数 agent_set2
typedef struct { 整 slot; 整 budget; } Agent;

空 si(整 c) {
    outb(c+1,0); outb(c+3,0x80); outb(c+0,1); outb(c+1,0);
    outb(c+3,3); outb(c+2,0xC7); outb(c+4,0x0B);
}
空 sw(整 c) { 循环((inb(c+5)&0x20)==0){} }
空 spc(整 c,整 v) { 若(v==10){sw(c);outb(c,13);} sw(c);outb(c,v); }
空 sps(整 c,字*s) { 循环(*s!=0){spc(c,*s);s++;} }
空 spn(整 c,整 n) {
    若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}
    若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}
    若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}
    spc(c,48+n);
}

// 用2参数初始化 (绕过r8 bug)
空 set2(Agent *a, 整 s) { a->slot = s; a->budget = 0; }  // budget 单独设
空 setb(Agent *a, 整 b) { a->budget = b; }

空 show(整 c, Agent *a, 字* n) {
    sps(c,n); sps(c," s="); spn(c,a->slot);
    sps(c," b="); spn(c,a->budget); spc(c,10);
}

空 _start(空) {
    整 c=0x3F8; si(c);
    sps(c,"JIAYAN v8 GATE4\nSCHEDULER | SEED:828\n");

    Agent a0,a1,a2,a3;
    a0.slot=0; a0.budget=128;        // 首个struct直接赋值OK
    set2(&a1,1); setb(&a1,255);      // 255 ≈ 4096/16 缩放演示
    set2(&a2,2); setb(&a2,64);       // 64 ≈ 1024/16
    set2(&a3,3); setb(&a3,0);

    show(c,&a0,"think "); show(c,&a1,"code  ");
    show(c,&a2,"review"); show(c,&a3,"test  ");

    整 cur=0,tok=8,r=0;  // tok=8 加速 (每tick 1)
    循环 (r<4) {
        若(cur==0)show(c,&a0,"think ");
        若(cur==1)show(c,&a1,"code  ");
        若(cur==2)show(c,&a2,"review");
        若(cur==3)show(c,&a3,"test  ");
        tok=tok-1;
        若(tok<=0){
            cur=cur+1;
            若(cur>=4){cur=0;r=r+1;若(r<4){sps(c,"---R");spc(c,48+r);spc(c,10);}}
            若(cur==0)tok=8;若(cur==1)tok=255;
            若(cur==2)tok=64;若(cur==3)tok=0;
        }
    }
    sps(c,"SCHED OK\n");
    循环(1){__asm(0xF4);}
}
