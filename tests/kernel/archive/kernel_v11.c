// 甲言内核 v11 — Gate 6 工具表 (简化: 无全局数组)
typedef struct { 整 id; 整 caps; } Tool;

空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}

空 caps_show(整 c, 整 caps){
    若(caps & 1) sps(c,"R");
    若(caps & 2) sps(c,"W");
    若(caps & 4) sps(c,"X");
    若(caps & 8) sps(c,"N");
}

整 can_use(Tool *t, 整 agent_caps){
    整 need = t->caps;
    返 (agent_caps & need) == need;
}

空 tool_check(整 c, Tool *t, 字* name, 整 a_caps, 字* a_name){
    sps(c,"  "); sps(c,name);
    sps(c," caps="); caps_show(c,t->caps);
    sps(c," "); sps(c,a_name); sps(c,":");
    sps(c, can_use(t,a_caps)?"YES":"NO");
    spc(c,10);
}

// === 入口 ===
空 _start(空){
    整 co=0x3F8;si(co);sps(co,"JIAYAN v11 GATE6\n");

    // 工具定义 (栈上, 6个)
    Tool compile, search, push, test, read, scan;
    compile.id=0; compile.caps=2;     // W
    search.id=1;  search.caps=1;      // R
    push.id=2;    push.caps=2|8;      // WN
    test.id=3;    test.caps=4;        // X
    read.id=4;    read.caps=1;        // R
    scan.id=5;    scan.caps=1;        // R

    // Agent 能力
    整 tc=1|8;           // think: R+N
    整 cc=1|2|4;         // code:  R+W+X
    整 rc=1;             // review: R
    整 xc=4;             // test: X

    sps(co,"AGENTS:");
    sps(co," think=");caps_show(co,tc);
    sps(co," code=");caps_show(co,cc);
    sps(co," review=");caps_show(co,rc);
    sps(co," test=");caps_show(co,xc);
    spc(co,10);
    sps(co,"TOOLS:\n");

    tool_check(co,&compile,"compile",tc,"think");
    tool_check(co,&compile,"compile",cc,"code");
    tool_check(co,&push,   "push",   tc,"think");
    tool_check(co,&push,   "push",   cc,"code");
    tool_check(co,&test,   "test",   xc,"test");
    tool_check(co,&read,   "read",   rc,"review");

    sps(co,"TOOLS OK\n");
    循环(1){__asm(0xF4);}
}
