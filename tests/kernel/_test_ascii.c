// Pure ASCII test - no Chinese characters
void si(int c){outb(c+1,0);outb(c+3,0x80);outb(c+0,1);outb(c+1,0);outb(c+3,3);outb(c+2,0xC7);outb(c+4,0x0B);}
void sw(int c){while((inb(c+5)&0x20)==0){}}
void spc(int c,int v){if(v==10){sw(c);outb(c,13);}sw(c);outb(c,v);}
void sps(int c,char*s){while(*s!=0){spc(c,*s);s++;}}
void spn(int c,int n){if(n>=1000){spc(c,48+n/1000);n=n-(n/1000)*1000;}if(n>=100){spc(c,48+n/100);n=n-(n/100)*100;}if(n>=10){spc(c,48+n/10);n=n-(n/10)*10;}spc(c,48+n);}

void _start(void){
    int c=0x3F8;si(c);sps(c,"ASCII\n");
    spn(c, 100); spc(c,10);
    spn(c, 99); spc(c,10);
    spn(c, 101); spc(c,10);
    while(1){__asm(0xF4);}
}
