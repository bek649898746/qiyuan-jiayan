// v44 — Gate 6 工具表: 编译时工具注册 + 按名分派 + 编译代码入口
// 工具签名: fn(整 argc, 字 **argv) → 整
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}
空 spn(整 c,整 n){若(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){spc(c,48);}}若(n>=100){spc(c,48+n/100);n=n-(n/100)*100;若(n<10){spc(c,48);}}若(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

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
    sps(c,"[ver]kernel-v44\n");
    返 0;
}
整 tool_compile(整 c, 整 argc, 字 **argv){
    sps(c,"[compile]");
    若(argc>=1){sps(c,argv[0]);}否则{sps(c,"(no-src)");}
    sps(c," COMPILE-STUB\n");
    返 0;
}

/* ---- 工具表 (并行数组: 名 + 函数指针, 低AST节点) ---- */
#define MAX_TOOLS 8
字 tool_names[MAX_TOOLS][16] = {"echo","add","ver","compile",""};
整 (*tool_fns[MAX_TOOLS])(整,整,字**) = {tool_echo,tool_add,tool_ver,tool_compile,0};

/* ---- 按名分派: 返 0=找到并执行, -1=未找到 ---- */
整 tool_dispatch(整 c, 字 *name, 整 argc, 字 **argv){
    整 i=0;
    循环(i<MAX_TOOLS){
        若(tool_fns[i]==0)断;
        字 *tn=tool_names[i];
        整 match=1; 整 j=0;
        循环(name[j]&&tn[j]){若(name[j]!=tn[j]){match=0;break;}j=j+1;}
        若(match&&name[j]==0&&tn[j]==0){
            返 tool_fns[i](c,argc,argv);
        }
        i=i+1;
    }
    返 -1;
}
/* 列出工具表 */
空 tool_list(整 c){
    sps(c,"[tools]");
    整 i=0;
    循环(i<MAX_TOOLS){
        若(tool_fns[i]==0)断;
        spc(c,32);sps(c,tool_names[i]);
        i=i+1;
    }
    spc(c,10);
}

空 _start(空){
    整 c=0x3F8;si(c);sps(c,"v44\n");
    /* 测试1: 列出工具表 */
    tool_list(c);
    /* 测试2: 分派 echo (argv: "hi" "there") */
    字 *a1[2]; a1[0]="hi"; a1[1]="there";
    tool_dispatch(c,"echo",2,a1);
    /* 测试3: 分派 add "3" "4" */
    字 *a2[2]; a2[0]="3"; a2[1]="4";
    tool_dispatch(c,"add",2,a2);
    /* 测试4: ver */
    tool_dispatch(c,"ver",0,0);
    /* 测试5: compile 入口 */
    字 *a3[1]; a3[0]="hello.jy";
    tool_dispatch(c,"compile",1,a3);
    /* 测试6: 未找到工具 */
    整 r=tool_dispatch(c,"nosuch",0,0);
    sps(c,"[nf]");spn(c,r);spc(c,10);
    /* 判定 */
    整 ok=1;
    若(r!=-1){ok=0;}
    若(tools[0].fn==0){ok=0;}
    若(ok==1){sps(c,"TOOLS-PASS\n");}否则{sps(c,"TOOLS-FAIL\n");}
    循环(1){__asm(0xF4);}
}
