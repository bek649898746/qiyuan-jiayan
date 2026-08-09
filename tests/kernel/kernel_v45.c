// v45 — Gate 6 工具表 v2: 固定 RAM 区注册 (零全局, bin 安全)
// 表布局: @0x2000000: names[8][16], fns[8] (每项: 名16字节+fn指针4字节)
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

#define TOOL_BASE 0x2000000
#define MAX_TOOLS 8

/* ---- 工具实现 ---- */
整 tool_echo(整 c, 整 argc, 字 **argv){
    sps(c,"[echo]");
    整 i=0;
    循环(i<argc){spc(c,32);sps(c,argv[i]);i=i+1;}
    spc(c,10);
    返 0;
}
整 tool_add(整 c, 整 argc, 字 **argv){
    整 a=0; 字 *p=argv[0]; 循环(*p){a=a*10+(*p)-48;p=p+1;}
    整 b=0; p=argv[1]; 循环(*p){b=b*10+(*p)-48;p=p+1;}
    sps(c,"[add]");spn(c,a+b);spc(c,10);
    返 0;
}
整 tool_ver(整 c, 整 argc, 字 **argv){
    sps(c,"[ver]kernel-v45\n");
    返 0;
}
整 tool_compile(整 c, 整 argc, 字 **argv){
    sps(c,"[compile]");
    若(argc>=1){sps(c,argv[0]);}否则{sps(c,"(no-src)");}
    sps(c," COMPILE-STUB\n");
    返 0;
}

/* ---- 注册: idx 槽写名+fn ---- */
整 tool_reg(整 base, 整 idx, 字 *name, 整 fn){
    若(idx>=MAX_TOOLS)返 -1;
    整 ent=base+idx*20;
    整 j=0; 循环(name[j]&&j<15){*(字节*)(ent+j)=name[j];j=j+1;}
    *(字节*)(ent+j)=0;
    *(整*)(ent+16)=fn;
    返 0;
}
/* ---- 分派: 扫描表, 名匹配则调用 ---- */
整 tool_dispatch(整 c, 整 base, 字 *name, 整 argc, 字 **argv){
    整 i=0;
    循环(i<MAX_TOOLS){
        整 ent=base+i*20;
        整 fn=*(整*)(ent+16);
        若(fn==0)断;
        字 *tn=(字*)ent;
        整 match=1; 整 j=0;
        循环(name[j]&&tn[j]){若(name[j]!=tn[j]){match=0;break;}j=j+1;}
        若(match&&name[j]==0&&tn[j]==0){
            /* 调用 fn(c, argc, argv): fn 是函数地址 */
            整 (*fp)(整,整,字**)=(整 (*)(整,整,字**))fn;
            返 fp(c,argc,argv);
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
    整 c=0x3F8;si(c);sps(c,"v45\n");
    整 base=TOOL_BASE;
    整 i=0;
    循环(i<MAX_TOOLS*20){*(字节*)(base+i)=0;i=i+1;}
    /* 注册 4 个工具 */
    tool_reg(base,0,"echo",(整)tool_echo);
    tool_reg(base,1,"add",(整)tool_add);
    tool_reg(base,2,"ver",(整)tool_ver);
    tool_reg(base,3,"compile",(整)tool_compile);
    tool_list(c,base);
    /* 诊断: 直接调用 tool_echo */
    字 *a1[2]; a1[0]="hi"; a1[1]="there";
    sps(c,"[direct]");
    tool_echo(c,2,a1);
    /* 诊断: 打印表内 fn 值 + 间接调用 */
    sps(c,"[fnval]");spn(c,*(整*)(base+16));spc(c,10);
    sps(c,"[fp]" );  /* 用分派调 echo */
    tool_dispatch(c,base,"echo",2,a1);
    /* add 3 4 */
    字 *a2[2]; a2[0]="3"; a2[1]="4";
    tool_dispatch(c,base,"add",2,a2);
    /* ver */
    tool_dispatch(c,base,"ver",0,0);
    /* compile 入口 */
    字 *a3[1]; a3[0]="hello.jy";
    tool_dispatch(c,base,"compile",1,a3);
    /* 未找到 */
    整 r=tool_dispatch(c,base,"nosuch",0,0);
    sps(c,"[nf]");spn(c,r);spc(c,10);
    /* 判定 */
    整 ok=1;
    若(r!=-1){ok=0;}
    若(*(整*)(base+16)==0){ok=0;}
    若(ok==1){sps(c,"TOOLS-PASS\n");}否则{sps(c,"TOOLS-FAIL\n");}
    循环(1){__asm(0xF4);}
}
