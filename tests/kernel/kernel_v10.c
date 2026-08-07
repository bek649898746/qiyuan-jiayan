// 甲言内核 v10 — Gate 5 Tensor持久池 (内联版)
// 构建: qcc_x86.exe -bin tests/kernel/kernel_v10.c -o scratch_test/kernel_v10.bin

typedef struct { 整 slot; 整 budget; 整 val; } Ag;

空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

// === Tensor池: 简单哈希表 (8槽, 内联) ===
#define TP_SZ 8

typedef struct {
    字 key[32];
    整 offset;
} TSlot;

// djb2 哈希
整 thash(字 *k){
    整 h=5381;
    循环(*k){ h=((h<<5)+h)+*k; k++; }
    返 h & (TP_SZ-1);
}

// 存
整 tput(TSlot *t, 字 *k){
    整 i=thash(k);
    循环(1){
        若(t[i].key[0]==0){
            整 j=0;循环(k[j]&&j<31){t[i].key[j]=k[j];j++;}t[i].key[j]=0;
            t[i].offset=100+i*10;
            返 1;
        }
        i=(i+1)&(TP_SZ-1);
        若(i==thash(k)) 返 0; // 表满
    }
}

// 取
整 tget(TSlot *t, 字 *k){
    整 i=thash(k);
    循环(1){
        若(t[i].key[0]==0) 返 -1;
        整 m=1,j=0;循环(k[j]&&j<31){若(t[i].key[j]!=k[j]){m=0;break;}j++;}
        若(m && t[i].key[j]==0) 返 t[i].offset;
        i=(i+1)&(TP_SZ-1);
    }
}

// === 入口 ===
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"JIAYAN v10 GATE5\n");

    TSlot pool[TP_SZ];
    整 i=0;循环(i<TP_SZ){pool[i].key[0]=0;i++;}

    // 存
    tput(pool, "weights");
    tput(pool, "kv_cache");
    tput(pool, "code_idx");

    // 取
    整 w = tget(pool, "weights");
    整 k = tget(pool, "kv_cache");
    整 ci= tget(pool, "code_idx");
    整 nx= tget(pool, "nonexist");

    sps(c,"weights=");spn(c,w);spc(c,10);
    sps(c,"kv_cache=");spn(c,k);spc(c,10);
    sps(c,"code_idx=");spn(c,ci);spc(c,10);
    sps(c,"nonexist=");spn(c,nx);spc(c,10);

    sps(c,"TPOOL OK\n");
    循环(1){__asm(0xF4);}
}
