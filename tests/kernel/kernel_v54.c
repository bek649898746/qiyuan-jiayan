// v54 — Gate 6 工具表 v5: 真函数指针分派 (fnptr cast 修复后, 原设计可用)
#include "srclib/kernel/serial.h"

#define TOOL_BASE 0x2000000
#define MAX_TOOLS 8

整 tool_echo(整 c){serial_puts(c,"[echo-ok]\n");返 0;}
整 tool_add(整 c){serial_puts(c,"[add-ok]\n");返 0;}
整 tool_ver(整 c){serial_puts(c,"[ver]kernel-v54\n");返 0;}
整 tool_compile(整 c){serial_puts(c,"[compile] COMPILE-STUB\n");返 0;}

/* 注册: idx 槽: 名@ent, fn@ent+16 */
整 tool_reg(整 base, 整 idx, 字 *name, 整 fn){
    若(idx>=MAX_TOOLS)返 -1;
    整 ent=base+idx*20;
    整 j=0; 循环(name[j]&&j<15){*(字节*)(ent+j)=name[j];j=j+1;}
    *(字节*)(ent+j)=0;
    *(整*)(ent+16)=fn;
    返 0;
}
/* 分派: 名匹配 → 函数指针间接调用 */
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
            整 (*fp)(整)=(整 (*)(整))fn;   /* fnptr cast — 2026-08-10 修复后可用 */
            返 fp(c);
        }
        i=i+1;
    }
    返 -1;
}
空 tool_list(整 c, 整 base){
    serial_puts(c,"[tools]");
    整 i=0;
    循环(i<MAX_TOOLS){
        整 ent=base+i*20;
        若(*(整*)(ent+16)==0)断;
        serial_putc(c,32);serial_puts(c,(字*)ent);
        i=i+1;
    }
    serial_putc(c,10);
}

空 _start(空){
    整 c=0x3F8;
    serial_init(c);
    serial_puts(c,"v54\n");
    整 base=TOOL_BASE;
    整 i=0;
    循环(i<MAX_TOOLS*20){*(字节*)(base+i)=0;i=i+1;}
    /* 注册 (fnptr cast 存函数地址) */
    tool_reg(base,0,"echo",(整)tool_echo);
    tool_reg(base,1,"add",(整)tool_add);
    tool_reg(base,2,"ver",(整)tool_ver);
    tool_reg(base,3,"compile",(整)tool_compile);
    tool_list(c,base);
    整 r1=tool_dispatch(c,base,"echo");
    整 r2=tool_dispatch(c,base,"ver");
    整 r3=tool_dispatch(c,base,"compile");
    整 r4=tool_dispatch(c,base,"nosuch");
    serial_puts(c,"[r]");
    serial_putc(c,48+r1);serial_putc(c,48+r2);serial_putc(c,48+r3);
    若(r4==-1){serial_puts(c,"nf");}否则{serial_putc(c,48+r4);}
    serial_putc(c,10);
    整 ok=1;
    若(r1!=0||r2!=0||r3!=0){ok=0;}
    若(r4!=-1){ok=0;}
    若(ok==1){serial_puts(c,"TOOLS-PASS\n");}否则{serial_puts(c,"TOOLS-FAIL\n");}
    循环(1){__asm(0xF4);}
}
