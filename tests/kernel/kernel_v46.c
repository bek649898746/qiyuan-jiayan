// v46 — Gate 6 工具表 v3: 固定RAM区注册 + 简单签名分派 (零全局, bin安全)
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}

#define TOOL_BASE 0x2000000
#define MAX_TOOLS 8

/* 工具: 统一签名 fn(整 serial) → 整 */
整 tool_echo(整 c){sps(c,"[echo-ok]\n");返 0;}
整 tool_add(整 c){sps(c,"[add-ok]\n");返 0;}
整 tool_ver(整 c){sps(c,"[ver]kernel-v46\n");返 0;}
整 tool_compile(整 c){sps(c,"[compile] COMPILE-STUB\n");返 0;}

/* 注册: 槽 idx: 名@ent, fn@ent+16 */
整 tool_reg(整 base, 整 idx, 字 *name, 整 fn){
    若(idx>=MAX_TOOLS)返 -1;
    整 ent=base+idx*20;
    整 j=0; 循环(name[j]&&j<15){*(字节*)(ent+j)=name[j];j=j+1;}
    *(字节*)(ent+j)=0;
    *(整*)(ent+16)=fn;
    返 0;
}
/* 分派: 名匹配 → fn(c) */
整 tool_dispatch(整 c, 整 base, 字 *name){
    整 i=0;
    循环(i<MAX_TOOLS){
        整 ent=base+i*20;
        整 fn=*(整*)(ent+16);
        若(fn==0)断;
        字 *tn=(字*)ent;
        整 match=1; 整 j=0;
        循环(name[j]&&tn[j]){若(name[j]!=tn[j]){match=0;break;}j=j+1;}
        若(match&&name[j]==0&&tn[j]==0){
            整 (*fp)(整)=(整 (*)(整))fn;
            返 fp(c);
        }
        i=i+1;
    }
    返 -1;
}
空 tool_list(整 c, 整 base){
    sps(c,"[tools]");
    整 i=0;
    循环(i<MAX_TOOLS){
        整 ent=base+i*20;
        若(*(整*)(ent+16)==0)断;
        spc(c,32);sps(c,(字*)ent);
        i=i+1;
    }
    spc(c,10);
}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v46\n");
    整 base=TOOL_BASE;
    整 i=0;
    循环(i<MAX_TOOLS*20){*(字节*)(base+i)=0;i=i+1;}
    tool_reg(base,0,"echo",(整)tool_echo);
    tool_reg(base,1,"add",(整)tool_add);
    tool_reg(base,2,"ver",(整)tool_ver);
    tool_reg(base,3,"compile",(整)tool_compile);
    tool_list(c,base);
    /* 分派各工具 */
    整 r1=tool_dispatch(c,base,"echo");
    sps(c,"[r1]");spc(c,48+r1+10);spc(c,10);  /* 0x30+0 */
    整 r2=tool_dispatch(c,base,"ver");
    整 r3=tool_dispatch(c,base,"compile");
    整 r4=tool_dispatch(c,base,"nosuch");
    sps(c,"[nf]");spc(c,48+r4+10);spc(c,10);
    /* 判定 */
    整 ok=1;
    若(r1!=0){ok=0;}
    若(r4!=-1){ok=0;}
    若(*(整*)(base+16)==0){ok=0;}
    若(ok==1){sps(c,"TOOLS-PASS\n");}否则{sps(c,"TOOLS-FAIL\n");}
    循环(1){__asm(0xF4);}
}
