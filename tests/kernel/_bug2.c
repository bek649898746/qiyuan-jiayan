// Bug #2 修复验证: struct 含 字* + 使用
空 si(整 c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
空 sw(整 c){循环((inb(c+5)&0x20)==0){}}
空 spc(整 c,整 v){若(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
空 sps(整 c,字*s){循环(*s!=0){spc(c,*s);s++;}}

typedef struct { 整 id; 字 *name; } Item;

空 _start(空){
    整 c=0x3F8;si(c);

    Item it;
    it.id = 42;
    it.name = "test";

    若(it.id == 42){ sps(c,"ID=42\n"); }
    否则{ sps(c,"ID!=42\n"); }

    sps(c, it.name);
    spc(c, 10);

    循环(1){__asm(0xF4);}
}
