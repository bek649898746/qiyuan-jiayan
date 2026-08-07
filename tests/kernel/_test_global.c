// Bug #1 修复验证: 读写全局变量
整 g_val = 42;
整 g_pos = 0;

空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}

空 _start(空){
    整 c=0x3F8;si(c);

    若(g_val == 42){ sps(c,"R=42\n"); }
    否则{ sps(c,"R!=42\n"); }

    g_val = 100;
    若(g_val == 100){ sps(c,"W=100\n"); }
    否则{ sps(c,"W!=100\n"); }

    g_pos = g_pos + 1;
    若(g_pos == 1){ sps(c,"INC=1\n"); }
    否则{ sps(c,"INC!=1\n"); }

    循环(1){__asm(0xF4);}
}
