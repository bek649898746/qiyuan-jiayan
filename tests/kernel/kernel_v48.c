// v48 — Gate 6 工具表 v4: 注册表(名) + 索引直接调用 (绕开 bin 间接调用 bug)
// 表: names@RAM (8×16), 分派: 名匹配→索引→switch 直接调用
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

#define TOOL_BASE 0x2000000
#define MAX_TOOLS 8

/* 工具实现 */
整 tool_echo(整 c){sps(c,"[echo-ok]\n");返 0;}
整 tool_add(整 c){sps(c,"[add-ok]\n");返 0;}
整 tool_ver(整 c){sps(c,"[ver]kernel-v48\n");返 0;}
整 tool_compile(整 c){sps(c,"[compile] COMPILE-STUB\n");返 0;}

/* 注册: 写名到表 */
整 tool_reg(整 base, 整 idx, 字 *name){
    若(idx>=MAX_TOOLS)返 -1;
    整 ent=base+idx*16;
    整 j=0; 循环(name[j]&&j<15){*(字节*)(ent+j)=name[j];j=j+1;}
    *(字节*)(ent+j)=0;
    返 0;
}
/* 名匹配 → 索引 (-1 未找到) */
整 tool_find(整 base, 字 *name){
    整 i=0;
    循环(i<MAX_TOOLS){
        整 ent=base+i*16;
        若(*(字节*)ent==0)断;
        字 *tn=(字*)ent;
        整 match=1; 整 j=0;
        循环(name[j]&&tn[j]){若(name[j]!=tn[j]){match=0;break;}j=j+1;}
        若(match&&name[j]==0&&tn[j]==0)返 i;
        i=i+1;
    }
    返 -1;
}
/* 索引 → 直接调用 (switch, 绕开 bin 间接调用 bug) */
整 tool_call(整 c, 整 idx){
    若(idx==0){返 tool_echo(c);}
    若(idx==1){返 tool_add(c);}
    若(idx==2){返 tool_ver(c);}
    若(idx==3){返 tool_compile(c);}
    返 -1;
}
空 tool_list(整 c, 整 base){
    sps(c,"[tools]");
    整 i=0;
    循环(i<MAX_TOOLS){
        整 ent=base+i*16;
        若(*(字节*)ent==0)断;
        spc(c,32);sps(c,(字*)ent);
        i=i+1;
    }
    spc(c,10);
}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v48\n");
    整 base=TOOL_BASE;
    整 i=0;
    循环(i<MAX_TOOLS*16){*(字节*)(base+i)=0;i=i+1;}
    tool_reg(base,0,"echo");
    tool_reg(base,1,"add");
    tool_reg(base,2,"ver");
    tool_reg(base,3,"compile");
    tool_list(c,base);
    /* 分派: 名→索引→调用 */
    整 i1=tool_find(base,"echo");
    整 r1=tool_call(c,i1);
    整 i2=tool_find(base,"ver");
    整 r2=tool_call(c,i2);
    整 i3=tool_find(base,"compile");
    整 r3=tool_call(c,i3);
    整 i4=tool_find(base,"nosuch");
    整 r4=tool_call(c,i4);
    sps(c,"[r]");spn(c,r1);spn(c,r2);spn(c,r3);spn(c,r4);spc(c,10);
    /* 判定 */
    整 ok=1;
    若(r1!=0){ok=0;}
    若(r4!=-1){ok=0;}
    若(i4!=-1){ok=0;}
    若(*(字节*)base==0){ok=0;}
    若(ok==1){sps(c,"TOOLS-PASS\n");}否则{sps(c,"TOOLS-FAIL\n");}
    循环(1){__asm(0xF4);}
}
