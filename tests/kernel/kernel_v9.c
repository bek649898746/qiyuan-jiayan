// 甲言内核 v9 — 依赖感知调度器 (Agent budget递减)

typedef struct { 整 slot; 整 budget; 整 val; } Ag; // val: bit0=started bit1=done

空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

空 set(Ag *a,整 s,整 b){ a->slot=s; a->budget=b; a->val=0; }
空 show(整 c,Ag *a,字* n){
    sps(c,n); sps(c," s="); spn(c,a->slot);
    sps(c," b="); spn(c,a->budget);
    若(a->val & 2) sps(c," DONE"); spc(c,10);
}

// === 入口 ===
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"JIAYAN v9 DEP\n");

    Ag a0,a1,a2,a3;
    set(&a0,0,2);   // think: 2 ticks (加速演示)
    set(&a1,1,4);   // code:  4 ticks
    set(&a2,2,2);   // review: 2 ticks
    set(&a3,3,2);   // test:  2 ticks

    show(c,&a0,"think "); show(c,&a1,"code  ");
    show(c,&a2,"review"); show(c,&a3,"test  ");

    整 round=0, cur=0;

    循环(round < 8){
        整 can=0;
        若(cur==0)can=1;                       // think: no deps
        若(cur==1)can=((a0.val&2)!=0);          // code ← think done
        若(cur==2)can=((a1.val&2)!=0);          // review ← code done
        若(cur==3)can=((a1.val&2)!=0);          // test ← code done

        若(can){
            Ag *a; 若(cur==0)a=&a0;若(cur==1)a=&a1;若(cur==2)a=&a2;若(cur==3)a=&a3;

            若((a->val & 2) == 0){  // not done yet
                若((a->val & 1) == 0){ show(c,a,"RUN  "); a->val|=1; } // first run: announce
                a->budget = a->budget - 1;
                若(a->budget <= 0){ a->val|=2; sps(c,"  -> DONE\n"); }
            }
        }

        cur=cur+1;
        若(cur>=4){ cur=0; round=round+1; 若(round<8){sps(c,"---R");spc(c,48+round);spc(c,10);} }
    }

    sps(c,"SCHED OK\n"); 循环(1){__asm(0xF4);}
}
