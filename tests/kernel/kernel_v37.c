// v37 — Tensor 持久池 v2: 版本链 + 追加写 (纯内存, QEMU 可验证)
// 布局: 池基址 0x2000000 (32MB), 索引表@池基, 数据块@池基+偏移
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

#define TENTRIES 64
#define TBLOCK (1024*1024)
/* 条目: name[32] @0, offset @32, size @36, version @40, prev @44, flags @48 (0空/1用) */
#define TPOOL_BASE 0x2000000

/* 哈希 djb2 */
整 tphash(字 *key){
    整 h=5381;
    循环(*key){h=((h<<5)+h)+*key;key=key+1;}
    返 h&63;
}
/* 找 key 的最大版本条目, 返索引 (-1 无) */
整 tp_find(整 tbl, 字 *key, 整 wantmax, 整 wantv){
    整 best=-1; 整 bestv=-1;
    整 i=0;
    循环(i<TENTRIES){
        整 fl=*(整*)(tbl+i*52+48);
        若(fl==1){
            字 *nm=(字*)(tbl+i*52);
            整 match=1; 整 j=0;
            循环(key[j]&&j<31){若(nm[j]!=key[j]){match=0;break;}j=j+1;}
            若(match&&nm[j]==0){
                整 v=*(整*)(tbl+i*52+40);
                若(wantmax){若(v>bestv){bestv=v;best=i;}}
                否则{若(v==wantv){返 i;}}
            }
        }
        i=i+1;
    }
    返 wantmax?best:-1;
}
/* 存: 新 key → v1; 已存在 → 追加新版本. 返数据地址 (0=表满) */
整 tpool_put(整 tbl, 字 *key, 整 size){
    整 idx=tphash(key);
    整 probe=0;
    循环(probe<TENTRIES){
        整 i=(idx+probe)&63;
        整 fl=*(整*)(tbl+i*52+48);
        若(fl==0){
            整 j=0; 循环(key[j]&&j<31){*(字节*)(tbl+i*52+j)=key[j];j=j+1;}
            *(字节*)(tbl+i*52+j)=0;
            整 v=1; 整 prev=-1;
            整 old=tp_find(tbl,key,1,0);
            若(old>=0){v=*(整*)(tbl+old*52+40)+1;prev=old;}
            *(整*)(tbl+i*52+32)=TPOOL_BASE+i*TBLOCK;  /* 数据块地址 */
            *(整*)(tbl+i*52+36)=size;
            *(整*)(tbl+i*52+40)=v;
            *(整*)(tbl+i*52+44)=prev;
            *(整*)(tbl+i*52+48)=1;
            返 TPOOL_BASE+i*TBLOCK;
        }
        probe=probe+1;
    }
    返 0;
}
/* 取最新版本数据地址 (0=未找到) */
整 tpool_get(整 tbl, 字 *key){
    整 i=tp_find(tbl,key,1,0);
    若(i>=0)返 *(整*)(tbl+i*52+32);
    返 0;
}
/* 取指定版本 (0=未找到) */
整 tpool_getv(整 tbl, 字 *key, 整 v){
    整 i=tp_find(tbl,key,0,v);
    若(i>=0)返 *(整*)(tbl+i*52+32);
    返 0;
}
/* 取版本号 (0=未找到) */
整 tpool_version(整 tbl, 字 *key){
    整 i=tp_find(tbl,key,1,0);
    若(i>=0)返 *(整*)(tbl+i*52+40);
    返 0;
}
/* 删: 全部版本 */
整 tpool_del(整 tbl, 字 *key){
    整 i=0; 整 found=0;
    循环(i<TENTRIES){
        整 fl=*(整*)(tbl+i*52+48);
        若(fl==1){
            字 *nm=(字*)(tbl+i*52);
            整 match=1; 整 j=0;
            循环(key[j]&&j<31){若(nm[j]!=key[j]){match=0;break;}j=j+1;}
            若(match&&nm[j]==0){*(整*)(tbl+i*52+48)=0;found=1;}
        }
        i=i+1;
    }
    返 found?0:-1;
}
空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v37t\n");
    整 tbl=TPOOL_BASE;
    /* 清零索引表 */
    整 i=0;
    循环(i<TENTRIES*52){*(字节*)(tbl+i)=0;i=i+1;}
    /* 测试1: 存 v1 */
    整 a=tpool_put(tbl,"hello",5);
    若(a==0){sps(c,"PUT1-FAIL\n");循环(1){__asm(0xF4);}}
    i=0;循环(i<5){*(字节*)(a+i)=65+i;i=i+1;}  /* "ABCDE" */
    sps(c,"P1 ");spn(c,tpool_version(tbl,"hello"));spc(c,10);
    /* 测试2: 追加 v2 */
    整 b=tpool_put(tbl,"hello",5);
    若(b==0){sps(c,"PUT2-FAIL\n");循环(1){__asm(0xF4);}}
    i=0;循环(i<5){*(字节*)(b+i)=75+i;i=i+1;}  /* "KLMNO" */
    sps(c,"P2 ");spn(c,tpool_version(tbl,"hello"));spc(c,10);
    /* 测试3: 取最新 = v2 */
    整 g=tpool_get(tbl,"hello");
    sps(c,"G=");
    i=0;循环(i<5){spc(c,*(字节*)(g+i));i=i+1;}
    spc(c,10);
    /* 测试4: 取 v1 */
    整 g1=tpool_getv(tbl,"hello",1);
    sps(c,"G1=");
    i=0;循环(i<5){spc(c,*(字节*)(g1+i));i=i+1;}
    spc(c,10);
    /* 测试5: 另一个 key */
    整 a2=tpool_put(tbl,"world",4);
    i=0;循环(i<4){*(字节*)(a2+i)=97+i;i=i+1;}  /* "abcd" */
    sps(c,"W=");
    整 gw=tpool_get(tbl,"world");
    i=0;循环(i<4){spc(c,*(字节*)(gw+i));i=i+1;}
    spc(c,10);
    /* 测试6: 删 hello */
    整 r=tpool_del(tbl,"hello");
    sps(c,"D=");spn(c,r);spc(c,10);
    整 gx=tpool_get(tbl,"hello");
    sps(c,"GX=");spn(c,gx);spc(c,10);
    整 gw2=tpool_get(tbl,"world");
    sps(c,"GW=");
    i=0;循环(i<4){spc(c,*(字节*)(gw2+i));i=i+1;}
    spc(c,10);
    /* 判定 */
    整 ok=1;
    若(tpool_version(tbl,"hello")!=0){ok=0;}
    若(gx!=0){ok=0;}
    若(*(字节*)(g+0)!=75){ok=0;}  /* 最新="K..." */
    若(*(字节*)(g1+0)!=65){ok=0;}  /* v1="A..." */
    若(*(字节*)(gw+0)!=97){ok=0;}
    若(*(字节*)(gw2+0)!=97){ok=0;}
    若(ok==1){sps(c,"TENSOR-PASS\n");}否则{sps(c,"TENSOR-FAIL\n");}
    循环(1){__asm(0xF4);}
}
