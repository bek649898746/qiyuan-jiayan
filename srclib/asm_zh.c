/* asm_zh.c - Chinese x86-64 Assembler v2
 * Parses .asm text (produced by qcc_x86 -S) and emits PE .exe
 * Usage: asm_zh input.asm [-o output.exe]
 * Supports named registers (rbp,rsp,rax,etc) and numeric (r0-r15)
 * seed=828 | 2026-08-03
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *code; static int cp;
static int data_rva_base = 0x2000;
static int exp_code_end;  /* expected code_end from qcc's .布局 directive */
static int exp_data_base; /* expected data_base from qcc */
static int asm_stc_n = 0; /* static slots (.堆计 N), for the heap counter */

/* ---- x86-64 emitters ---- */
static void b(unsigned char v) { code[cp++] = v; }
static void b4(int v) { b(v&0xff); b((v>>8)&0xff); b((v>>16)&0xff); b((v>>24)&0xff); }
static void modrm(int mod, int reg, int rm) { b((mod<<6)|((reg&7)<<3)|(rm&7)); }
static void rex(int w, int r, int x, int bv) { b(0x40|(w?8:0)|(r?4:0)|(x?2:0)|(bv?1:0)); }
static void b4_at(int pos, int v) { code[pos]=v&0xff;code[pos+1]=(v>>8)&0xff;code[pos+2]=(v>>16)&0xff;code[pos+3]=(v>>24)&0xff; }
static void b_at(int pos, int v) { code[pos]=v&0xff; }

static void mov_r_imm(int reg, int imm) { if(reg&8)b(0x41); b(0xB8|(reg&7)); b4(imm); } /* REX.B for r8-r15 (fix 2026-08-03) */
static void mov_rr64(int d, int s) { rex(1,s&8,0,d&8); b(0x89); modrm(3,s&7,d&7); }
static void mov_rr(int d, int s) { rex(0,s&8,0,d&8); b(0x89); modrm(3,s&7,d&7); }
static void mov_mbrp_reg(int disp, int reg) { rex(0,reg&8,0,0); b(0x89); if(disp<128&&disp>=-128){modrm(1,reg&7,5);b(disp);}else{modrm(2,reg&7,5);b4(disp);} }
static void mov_reg_mbrp(int reg, int disp) { rex(0,reg&8,0,0); b(0x8B); if(disp<128&&disp>=-128){modrm(1,reg&7,5);b(disp);}else{modrm(2,reg&7,5);b4(disp);} }
static void mov_mbrp_reg64(int disp, int reg) { rex(1,reg&8,0,0); b(0x89); if(disp<128&&disp>=-128){modrm(1,reg&7,5);b(disp);}else{modrm(2,reg&7,5);b4(disp);} }
static void mov_reg_mbrp64(int reg, int disp) { rex(1,reg&8,0,0); b(0x8B); if(disp<128&&disp>=-128){modrm(1,reg&7,5);b(disp);}else{modrm(2,reg&7,5);b4(disp);} }
static void mov_mrsp_reg64(int disp, int reg) { rex(1,reg&8,0,0); b(0x89); if(disp<128&&disp>=-128){modrm(1,reg&7,4);b(0x24);b(disp);}else{modrm(2,reg&7,4);b(0x24);b4(disp);} }
static void mov_reg_mrsp64(int reg, int disp) { rex(1,reg&8,0,0); b(0x8B); if(disp<128&&disp>=-128){modrm(1,reg&7,4);b(0x24);b(disp);}else{modrm(2,reg&7,4);b(0x24);b4(disp);} }
static void mov_eax_rip(int d) { b(0x8B);b(0x05);b4(d); }
static void mov_rip_eax(int d) { b(0x89);b(0x05);b4(d); } /* mov [rip+disp], eax (fix 2026-08-03) */
static void mov_rax_rip64(int d) { rex(1,0,0,0);b(0x8B);b(0x05);b4(d); }
static void mov_rip_rax64(int d) { rex(1,0,0,0);b(0x89);b(0x05);b4(d); }
static void mov_reg_mreg(int r, int m) { rex(0,r&8,0,m&8);b(0x8B);if(m==4||m==12){modrm(0,r&7,m&7);b(0x24);}else modrm(0,r&7,m&7); }
static void mov_reg_mreg64(int r, int m) { rex(1,r&8,0,m&8);b(0x8B);if(m==4||m==12){modrm(0,r&7,m&7);b(0x24);}else modrm(0,r&7,m&7); }
static void mov_mreg_reg(int m, int r) { rex(0,r&8,0,m&8);b(0x89);if(m==4||m==12){modrm(0,r&7,m&7);b(0x24);}else modrm(0,r&7,m&7); }
static void mov_eax_mr13(void) { b(0x41);b(0x8B);b(0x45);b(0); }
static void mov_rax_mr13(void) { b(0x49);b(0x8B);b(0x45);b(0); }
static void mov_r12_cl(void) { rex(0,0,0,1);b(0x88);modrm(0,1,4);b(0x24); }
static void mov_r12_al(void) { rex(0,0,0,1);b(0x88);modrm(0,0,4);b(0x24); }
static void movzx_eax_al(void) { b(0x0F);b(0xB6);modrm(3,0,0); }
static void lea_r_mrsp(int r, int d) { rex(1,r&8,0,0);b(0x8D);if(d<128&&d>=-128){modrm(1,r&7,4);b(0x24);b(d);}else{modrm(2,r&7,4);b(0x24);b4(d);} }
static void lea_r_mbrp(int r, int d) { rex(1,r&8,0,0);b(0x8D);if(d<128&&d>=-128){modrm(1,r&7,5);b(d);}else{modrm(2,r&7,5);b4(d);} }
static void lea_rax_rip(int d) { rex(1,0,0,0);b(0x8D);b(0x05);b4(d); }
static void alu_rr(int op, int d, int s) {
    /* Matches qcc direct-path emission: REX(0) with REX.R/B for high regs
       (qcc uses 32-bit opcode form, no REX.W). Fix 2026-08-03: REX was
       missing entirely for add/sub/cmp, breaking H1==H2 equivalence. */
    if(op==5){rex(0,s&8,0,d&8);b(0x01);modrm(3,s&7,d&7);}
    else if(op==6){rex(0,s&8,0,d&8);b(0x29);modrm(3,s&7,d&7);}
    else if(op==8){rex(0,s&8,0,d&8);b(0x39);modrm(3,s&7,d&7);}
    else if(op==7){rex(0,d&8,0,s&8);b(0x0F);b(0xAF);modrm(3,d&7,s&7);} /* imul reg=dst, rm=src: REX.R=dst REX.B=src (matches qcc) */
}
static void test_rr(int r1, int r2) { rex(0,r2&8,0,r1&8);b(0x85);modrm(3,r2&7,r1&7); }
static void setcc(int op) { unsigned char c=(op==8)?0x94:(op==10)?0x95:(op==11)?0x9C:(op==21)?0x9F:(op==22)?0x9E:(op==23)?0x9D:(op==31)?0x92:(op==32)?0x97:(op==33)?0x96:(op==34)?0x93:0x94;b(0x0F);b(c);modrm(3,0,0); }
static void push_r(int r) { if(r&8)b(0x41);b(0x50|(r&7)); }
static void pop_r(int r) { if(r&8)b(0x41);b(0x58|(r&7)); }
static void ret(void) { b(0xC3); }
static void sub_rsp_imm(int v) { b(0x48);b(0x81);b(0xEC);b4(v); }
static void add_rsp_imm(int v) { b(0x48);b(0x81);b(0xC4);b4(v); }
/* SSE/Float emitters */
static void movsd_xmm0_mbrp(int d) { b(0xF2);b(0x0F);b(0x10);if(d<128&&d>=-128){modrm(1,0,5);b(d);}else{modrm(2,0,5);b4(d);} }
static void movsd_xmm1_mbrp(int d) { b(0xF2);b(0x0F);b(0x10);if(d<128&&d>=-128){modrm(1,1,5);b(d);}else{modrm(2,1,5);b4(d);} }
static void movsd_mbrp_xmmreg(int d, int reg) { b(0xF2);b(0x0F);b(0x11);if(d<128&&d>=-128){modrm(1,reg&7,5);b(d);}else{modrm(2,reg&7,5);b4(d);} }
static void movsd_xmm0_xmm1(void) { b(0xF2);b(0x0F);b(0x10);b(0xC1); }
static void movsd_xmm1_xmm0(void) { b(0xF2);b(0x0F);b(0x10);b(0xC8); }
static void addsd_xmm0_xmm1(void) { b(0xF2);b(0x0F);b(0x58);b(0xC1); }
static void subsd_xmm0_xmm1(void) { b(0xF2);b(0x0F);b(0x5C);b(0xC1); }
static void mulsd_xmm0_xmm1(void) { b(0xF2);b(0x0F);b(0x59);b(0xC1); }
static void divsd_xmm0_xmm1(void) { b(0xF2);b(0x0F);b(0x5E);b(0xC1); }
static void cvtsi2sd_xmm0_eax(void) { b(0xF2);b(0x0F);b(0x2A);b(0xC0); }
static void cvtsi2sd_xmm1_eax(void) { b(0xF2);b(0x0F);b(0x2A);b(0xC8); }
static void cvttsd2si_eax_xmm0(void) { b(0xF2);b(0x0F);b(0x2C);b(0xC0); }
static void comisd_xmm0_xmm1(void) { b(0x66);b(0x0F);b(0x2F);b(0xC1); }
static void push_xmm0(void) { sub_rsp_imm(8);b(0xF2);b(0x0F);b(0x11);modrm(0,0,4);b(0x24); }
static void pop_xmm0(void) { b(0xF2);b(0x0F);b(0x10);modrm(0,0,4);b(0x24);add_rsp_imm(8); }

/* Label/patch system */
#define MAX_LABELS 4096
static int label_pos[MAX_LABELS], label_set[MAX_LABELS], nl;
static struct { char name[32]; int pos, defined, line; } labels[16384]; static int ln; /* fix 2026-08-03: was 4096 — qcc_x86.c self text has 6197 labels (.L#### + function names), overflow silently dropped 2586 jump instrs → H2 lost ~14KB. line: source line for error messages (fix 2026-08-05) */
static struct { int at, target, is_jmp; } patches[16384]; static int pn; /* fix 2026-08-03: was 8192 — forward-jump patch count for the self text exceeds it */
static void patch_label(int at, int tl, int is_jmp) { if(pn<16384){patches[pn].at=at;patches[pn].target=tl;patches[pn].is_jmp=is_jmp;pn++;} else { fprintf(stderr, "[ERR] patch table overflow (%d)\n", pn); exit(1); } }
static void call_rel(int rel) { b(0xE8);b4(rel); }
static void jmp_rel(int rel) { b(0xE9);b4(rel); }
static void jz_rel(int rel) { b(0x0F);b(0x84);b4(rel); }
static void jnz_rel(int rel) { b(0x0F);b(0x85);b4(rel); }
static void call_iat(int slot) { int at=cp; int iat_off = slot < 8 ? (8 + 8 * slot) : (0x50 + 8 * (slot - 8)); b(0xFF);b(0x15);b4((data_rva_base+iat_off)-(0x1000+at+6)); } /* IAT1@+0x08 / IAT2@+0x50（fix 2026-08-06 BUG-1） */
static void resolve_patches(void) { for(int i=0;i<pn;i++){ if(!labels[patches[i].target].defined){fprintf(stderr,"[ERR] asm_zh: undefined label '%s' at line %d\n",labels[patches[i].target].name,labels[patches[i].target].line);exit(1);} int t=labels[patches[i].target].pos;int at=patches[i].at; if(patches[i].is_jmp==3){ int v=t-(at+1); b_at(at,v&0xff); } else { int v=t-(at+4); b4_at(at,v); } } } /* fix 2026-08-05: rel8 (is_jmp==3) vs rel32 back-patch; undefined label → clean error */
/* Data section buffer */
static unsigned char *sdat; static int sdp, sdc;
/* string-address back-patch: STR{n} marker in "移动 r0, STRn" */
static struct { int at; int idx; } str_patches2[2048]; static int spn2;
static int str_offs2[1024]; static int str_cnt2; /* sdat offset of each .字串 by appearance order */
/* function-address back-patch: FN{name} marker in "移动 r0, FN:add" (fix 2026-08-03: -S path had no fn-addr patch, so fnptr assignments emitted mov eax,0 in H2) */
static struct { int at; int label; } fn_patches2[2048]; static int fnpn2;
/* double-literal RIP-rel back-patch: DBL{n} marker in "浮取静 xmm0, [rip+DBLn]" */
static struct { int at; int idx; } dbl_patches2[2048]; static int dpn2;
static int dbl_offs2[1024]; static int dbl_cnt2; /* sdat offset of each .浮点 by appearance order */

/* PE writer */
static void pad(FILE *f, int n) { while(n-->0)fputc(0,f); }
static void w4(FILE *f, int v) { fputc(v&0xff,f);fputc((v>>8)&0xff,f);fputc((v>>16)&0xff,f);fputc((v>>24)&0xff,f); }
static void w8(FILE *f, int v) { w4(f,v);w4(f,0); }
static void w2(FILE *f, int v) { fputc(v&0xff,f);fputc((v>>8)&0xff,f); }

static void write_pe(FILE *f, int entry_rva) {
        int ts=((cp+4095)&~4095); if(ts<512)ts=512;
    int tf=0x200, dr=data_rva_base;
    int isz=dr+0x5000000+0x1000, df=tf+ts;
    fputc('M',f);fputc('Z',f);pad(f,58);w4(f,64);
    fputc('P',f);fputc('E',f);fputc(0,f);fputc(0,f);
    w2(f,0x8664);w2(f,2);w4(f,0);w4(f,0);w4(f,0);w2(f,0xF0);w2(f,0x22E);
    w2(f,0x020B);fputc(0,f);fputc(0,f);w4(f,ts);w4(f,8);w4(f,0);w4(f,entry_rva);w4(f,0x1000);
    w8(f,0x400000);w4(f,0x1000);w4(f,0x200);w2(f,6);w2(f,0);w2(f,0);w2(f,0);w2(f,6);w2(f,0);
    w4(f,0);w4(f,isz);w4(f,0x200);w4(f,0);w2(f,3);w2(f,0x8100);
    w8(f,0x100000);w8(f,0x400000);w8(f,0x100000);w8(f,0x1000);w4(f,0);w4(f,16);
    w4(f,0);w4(f,0);w4(f,dr+0x1A8);w4(f,60); /* import dir: desc@+0x1A8（fix 2026-08-06 BUG-1） */
    for(int di=2;di<16;di++){w4(f,0);w4(f,0);}
    fwrite(".text\0\0\0",1,8,f);w4(f,ts);w4(f,0x1000);w4(f,ts);w4(f,tf);w4(f,0);w4(f,0);w2(f,0);w2(f,0);w4(f,0x60000020);
    fwrite(".data\0\0\0",1,8,f);w4(f,0x5000000);w4(f,dr);w4(f,0x4000);w4(f,df);w4(f,0);w4(f,0);w2(f,0);w2(f,0);w4(f,0xC0000040);
    int pos=(int)ftell(f);while(pos<tf)fputc(0,f),pos++;fwrite(code,1,cp,f);pos=(int)ftell(f);while(pos<df)fputc(0,f),pos++;
    /* .data header (match qcc fix 2026-08-06 BUG-1): heap counter@+0 (8B),
       IAT1@+8 (8 kernel32 + term), IAT2@+0x50 (16 msvcrt + term),
       ILT1@+0xD8, ILT2@+0x120, desc@+0x1A8, names@+0x1E4, statics@+0x300. */
    w8(f, 0x400000 + dr + 0x300 + 4 * asm_stc_n + 2560); /* heap counter */
    int nslot[24];
    /* IAT/ILT 占位 */
    fseek(f, df + 0x08, SEEK_SET); for(int i=0;i<9;i++)w8(f,0);
    fseek(f, df + 0x50, SEEK_SET); for(int i=0;i<17;i++)w8(f,0);
    fseek(f, df + 0xD8, SEEK_SET); for(int i=0;i<9;i++)w8(f,0);
    fseek(f, df + 0x120, SEEK_SET); for(int i=0;i<17;i++)w8(f,0);
    /* names（hint 2B + name + \0）逐个写 */
    const char *nm[]={"GetStdHandle","WriteFile","CreateFileA","ReadFile","VirtualAlloc","SetFilePointer","ExitProcess","GetCommandLineA"};
    const char *mn[]={"pow","atan2","fmod","sqrt","cos","sin","tan","acos","asin","atan","log","log10","exp","floor","ceil","fabs"};
    fseek(f, df + 0x1E4, SEEK_SET);
    for(int i=0;i<8;i++){nslot[i]=(int)ftell(f)-df;w2(f,0);fputs(nm[i],f);fputc(0,f);}
    for(int i=0;i<16;i++){nslot[8+i]=(int)ftell(f)-df;w2(f,0);fputs(mn[i],f);fputc(0,f);}
    int kdll=(int)ftell(f)-df;fputs("kernel32.dll",f);fputc(0,f);
    int mdll=(int)ftell(f)-df;fputs("msvcrt.dll",f);fputc(0,f);
    /* 回填 IAT1/IAT2 */
    fseek(f, df + 0x08, SEEK_SET); for(int i=0;i<8;i++)w8(f,dr+nslot[i]); w8(f,0);
    fseek(f, df + 0x50, SEEK_SET); for(int i=0;i<16;i++)w8(f,dr+nslot[8+i]); w8(f,0);
    /* 回填 ILT1/ILT2 */
    fseek(f, df + 0xD8, SEEK_SET); for(int i=0;i<8;i++)w8(f,dr+nslot[i]); w8(f,0);
    fseek(f, df + 0x120, SEEK_SET); for(int i=0;i<16;i++)w8(f,dr+nslot[8+i]); w8(f,0);
    /* desc: kernel32 (IAT1/ILT1), msvcrt (IAT2/ILT2) */
    fseek(f, df + 0x1A8, SEEK_SET);
    w4(f,dr+0xD8);w4(f,0);w4(f,0);w4(f,dr+kdll);w4(f,dr+0x08);
    w4(f,dr+0x120);w4(f,0);w4(f,0);w4(f,dr+mdll);w4(f,dr+0x50);
    for(int di=0;di<5;di++)w4(f,0);
    fseek(f,0,SEEK_END);
    pos=(int)ftell(f);while(pos<df+0x4000)fputc(0,f),pos++;fseek(f,0,SEEK_END);
}

/* ---- Assembler state ---- */

static int asm_assemble(const char *src, int base, int emit_data) {
    #define LF(n) ({ int li=-1;for(int i=0;i<ln;i++)if(!strcmp(labels[i].name,n)){li=i;break;}li;})
    data_rva_base = base;
    spn2=0; str_cnt2=0; memset(str_offs2,0,sizeof(str_offs2));
    dpn2=0; dbl_cnt2=0; memset(dbl_offs2,0,sizeof(dbl_offs2));
    fnpn2=0; /* function-address patches are per-pass too (fix 2026-08-03: pass1 residue pointed b4_at at stale offsets) */
    cp=0;pn=0;nl=0;ln=0;memset(label_pos,0,sizeof(label_pos));memset(label_set,0,sizeof(label_set));
    memset(labels,0,sizeof(labels)); /* clear label table (pass1 residue must not leak into pass2) */
    sdc=256;sdat=malloc(sdc);sdp=0;
    const char *p=src;
    #define SK while(*p==' '||*p=='\t'||*p=='\r')p++
    #define NUM ({int s=1,v=0;SK;if(*p=='-'){s=-1;p++;}if(*p=='0'&&p[1]=='x'){p+=2;while((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')){v=v*16+(*p<='9'?*p-'0':*p-'a'+10);p++;}}else while(*p>='0'&&*p<='9'){v=v*10+(*p-'0');p++;}s*v;})
    #define RG ({int r=-1;SK;if(*p=='r'&&p[1]>='0'&&p[1]<='9'){p++;r=0;while(*p>='0'&&*p<='9'){r=r*10+(*p-'0');p++;}}else if(!strncmp(p,"eax",3)){r=0;p+=3;}else if(!strncmp(p,"ecx",3)){r=1;p+=3;}else if(!strncmp(p,"edx",3)){r=2;p+=3;}else if(!strncmp(p,"ebx",3)){r=3;p+=3;}else if(!strncmp(p,"esp",3)){r=4;p+=3;}else if(!strncmp(p,"ebp",3)){r=5;p+=3;}else if(!strncmp(p,"esi",3)){r=6;p+=3;}else if(!strncmp(p,"edi",3)){r=7;p+=3;}else if(!strncmp(p,"rax",3)){r=0;p+=3;}else if(!strncmp(p,"rcx",3)){r=1;p+=3;}else if(!strncmp(p,"rdx",3)){r=2;p+=3;}else if(!strncmp(p,"rbx",3)){r=3;p+=3;}else if(!strncmp(p,"rsp",3)){r=4;p+=3;}else if(!strncmp(p,"rbp",3)){r=5;p+=3;}else if(!strncmp(p,"rsi",3)){r=6;p+=3;}else if(!strncmp(p,"rdi",3)){r=7;p+=3;}else if(!strncmp(p,"r8",2)){r=8;p+=2;}else if(!strncmp(p,"r9",2)){r=9;p+=2;}else if(!strncmp(p,"r10",3)){r=10;p+=3;}else if(!strncmp(p,"r11",3)){r=11;p+=3;}else if(!strncmp(p,"r12",3)){r=12;p+=3;}else if(!strncmp(p,"r13",3)){r=13;p+=3;}else if(!strncmp(p,"r14",3)){r=14;p+=3;}else if(!strncmp(p,"r15",3)){r=15;p+=3;} r;})
    #define MEM ({int d=0;SK;if(*p=='['){p++;SK;int br=RG;(void)br;SK;int sign=1;if(*p=='+')p++;else if(*p=='-'){sign=-1;p++;}d=NUM*sign;SK;if(*p==']')p++;} d;})
    /* RIP-relative: parse "[rip+N]" bare displacement (fix 2026-08-03:
       qcc -S emits 取静32/存静64/取静址 as "[rip+N]"; MEM's RG failed on
       "rip" so disp was always 0 -> H1!=H2). */
    #define RIPS ({int d=0;SK;if(*p=='['){p++;SK;if(!strncmp(p,"rip",3))p+=3;SK;int sign=1;if(*p=='+')p++;else if(*p=='-'){sign=-1;p++;}d=NUM*sign;SK;if(*p==']')p++;} d;})
    #define LR ({char lnm[32];int li=0;SK;while(*p&&*p!=' '&&*p!='\t'&&*p!='\n'&&*p!=','&&li<31)lnm[li++]=*p++;lnm[li]=0; \
        ({int li2=LF(lnm); \
          if(li2<0 && lnm[0] && ln<16384){ /* forward ref: auto-register label */ \
            strcpy(labels[ln].name,lnm);labels[ln].pos=0;labels[ln].defined=0;labels[ln].line=line_no;li2=ln++;} \
          else if(li2<0 && lnm[0]){ fprintf(stderr, "[ERR] label table overflow (%d) at '%s'\n", ln, lnm); exit(1); } \
          li2;});})
    int line_no = 1; /* fix 2026-08-05: line tracking for error messages (per-pass: pass1/pass2 both start at 1) */
    while(*p){
        while(*p==' '||*p=='\t'||*p=='\n'||*p=='\r'){ if(*p=='\n') line_no++; p++; } if(!*p)break;
        if(*p==';'){while(*p&&*p!='\n')p++;continue;}
        /* Peek ahead for label (contains ':'). A line whose text before ':' has a
           '"' is a .字串/.浮点 DIRECTIVE (string args like "Usage: ..."), NOT a label. */
        const char *ls=p; while(*p&&*p!=':'&&*p!='\n')p++;
        if(*p==':'){int hasq=0;int hsp=0;for(const char *qc=ls;qc<p;qc++){if(*qc=='"')hasq=1;if(*qc==' '||*qc=='\t')hsp=1;}if(!hasq&&!hsp){ /* ':' only defines a LABEL when the text before it has no whitespace — "移动 r0, FN:add" has a ':' in its operand (fix 2026-08-03) */
            int len=(int)(p-ls); if(len>0&&len<32&&ln<16384){char n[32];memcpy(n,ls,len);n[len]=0;
            int li=LF(n); if(li<0){strcpy(labels[ln].name,n);labels[ln].pos=cp;labels[ln].defined=1;labels[ln].line=line_no;li=ln++;}
            else{labels[li].pos=cp;labels[li].defined=1;}}
            else if(len>0){ fprintf(stderr, "[ERR] label def overflow len=%d ln=%d\n", len, ln); exit(1); }
            p++;
            while(*p&&*p!='\n'){p++;} if(*p=='\n'){p++;line_no++;} continue;
        }}
        /* Not a label: parse instruction */
        p=ls;
        char mn[32];int mi=0;while(*p&&*p!=' '&&*p!='\t'&&*p!='\r'&&*p!='\n'&&mi<31)mn[mi++]=*p++;mn[mi]=0; /* fix 2026-08-05: treat \r as delimiter (Windows CRLF hand-written files) */
        if(!strcmp(mn,"压栈")){int r=RG;if(r>=0)push_r(r);}
        else if(!strcmp(mn,"弹栈")){int r=RG;if(r>=0)pop_r(r);}
        else if(!strcmp(mn,"返回")){ret();}
        else if(!strcmp(mn,"扩字")){rex(1,0,0,0);b(0x98);} /* cdqe (fix 2026-08-05) */
        else if(!strcmp(mn,"扩八字")){rex(1,0,0,0);b(0x99);} /* cqo (fix 2026-08-05) */
        else if(!strcmp(mn,"串拷")){b(0xF3);b(0xA4);} /* rep movsb (fix 2026-08-05) */
     else if(!strcmp(mn,"串比")){b(0xA6);} /* cmpsb (P1) */
     else if(!strcmp(mn,"串比双")){b(0xA7);} /* cmpsd (P1) */
     else if(!strcmp(mn,"串扫")){b(0xAE);} /* scasb (P1) */
     else if(!strcmp(mn,"串扫双")){b(0xAF);} /* scasd (P1) */
     else if(!strcmp(mn,"串载")){b(0xAC);} /* lodsb (P1) */
     else if(!strcmp(mn,"串载双")){b(0xAD);} /* lodsd (P1) */
     else if(!strcmp(mn,"串存")){b(0xAA);} /* stosb (P1) */
     else if(!strcmp(mn,"串存双")){b(0xAB);} /* stosd (P1) */
     else if(!strcmp(mn,"串拷双")){b(0xF3);b(0xA5);} /* rep movsd (P1) */
        else if(!strcmp(mn,"交还")){SK;int m=RG;SK;if(*p==',')p++;SK;int r=RG;if(m>=0&&r>=0){b(0x0F);b(0xB1);modrm(3,r&7,m&7);}} /* cmpxchg r/m32,r32 (fix 2026-08-05) */
        else if(!strcmp(mn,"条移等")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){if(d>=8||s>=8)rex(0,s&8,0,d&8);b(0x0F);b(0x44);modrm(3,d&7,s&7);}} /* cmove (fix 2026-08-05) */
        else if(!strcmp(mn,"条移不等")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){if(d>=8||s>=8)rex(0,s&8,0,d&8);b(0x0F);b(0x45);modrm(3,d&7,s&7);}} /* cmovne */
        else if(!strcmp(mn,"条移低")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){if(d>=8||s>=8)rex(0,s&8,0,d&8);b(0x0F);b(0x42);modrm(3,d&7,s&7);}} /* cmovb */
        else if(!strcmp(mn,"条移低等")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){if(d>=8||s>=8)rex(0,s&8,0,d&8);b(0x0F);b(0x46);modrm(3,d&7,s&7);}} /* cmovbe */
        else if(!strcmp(mn,"条移高")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){if(d>=8||s>=8)rex(0,s&8,0,d&8);b(0x0F);b(0x47);modrm(3,d&7,s&7);}} /* cmova */
        else if(!strcmp(mn,"条移高等")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){if(d>=8||s>=8)rex(0,s&8,0,d&8);b(0x0F);b(0x43);modrm(3,d&7,s&7);}} /* cmovae */
        else if(!strcmp(mn,"条移小")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){if(d>=8||s>=8)rex(0,s&8,0,d&8);b(0x0F);b(0x4C);modrm(3,d&7,s&7);}} /* cmovl */
        else if(!strcmp(mn,"条移小等")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){if(d>=8||s>=8)rex(0,s&8,0,d&8);b(0x0F);b(0x4E);modrm(3,d&7,s&7);}} /* cmovle */
        else if(!strcmp(mn,"条移大")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){if(d>=8||s>=8)rex(0,s&8,0,d&8);b(0x0F);b(0x4F);modrm(3,d&7,s&7);}} /* cmovg */
        else if(!strcmp(mn,"条移负")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0x48);modrm(3,d&7,s&7);}} /* cmovs (P0) */
     else if(!strcmp(mn,"条移非负")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0x49);modrm(3,d&7,s&7);}} /* cmovns (P0) */
     else if(!strcmp(mn,"条移溢")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0x40);modrm(3,d&7,s&7);}} /* cmovo (P0) */
     else if(!strcmp(mn,"条移不溢")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0x41);modrm(3,d&7,s&7);}} /* cmovno (P0) */
     else if(!strcmp(mn,"条移奇")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0x4A);modrm(3,d&7,s&7);}} /* cmovp (P0) */
     else if(!strcmp(mn,"条移偶")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0x4B);modrm(3,d&7,s&7);}} /* cmovnp (P0) */
     else if(!strcmp(mn,"条移大等")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){if(d>=8||s>=8)rex(0,s&8,0,d&8);b(0x0F);b(0x4D);modrm(3,d&7,s&7);}} /* cmovge */
        else if(!strcmp(mn,"移动64")){SK;int r=RG;SK;if(*p==',')p++;int lo=NUM;SK;if(*p==',')p++;int hi=NUM;if(r>=0){b(0x48);b(0xB8);b4(lo);b4(hi);}} /* mov rax,imm64 (LL; fix 2026-08-05) */
     else if(!strcmp(mn,"移动")){SK;int r=RG;SK;if(*p==',')p++;SK;if(!strncmp(p,"STR",3)){p+=3;int si=atoi(p);if(r>=0&&spn2<2048){mov_r_imm(r,0);str_patches2[spn2].at=cp-4;str_patches2[spn2].idx=si;spn2++;}}else if(!strncmp(p,"FN:",3)){p+=3;int li=LR;if(r>=0&&fnpn2<2048&&li>=0){mov_r_imm(r,0);fn_patches2[fnpn2].at=cp-4;fn_patches2[fnpn2].label=li;fnpn2++;}}else{int v=NUM;if(r>=0)mov_r_imm(r,v);}}
        else if(!strcmp(mn,"移64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0)mov_rr64(d,s);}
        else if(!strcmp(mn,"移32")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0)mov_rr(d,s);}
     else if(!strcmp(mn,"交换")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x87);modrm(3,d&7,s&7);}} /* xchg r32,r/m32 (P0) */
     else if(!strcmp(mn,"交换64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x87);modrm(3,d&7,s&7);}} /* xchg r64,r/m64 (P0) */
        else if(!strcmp(mn,"与64")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x21);modrm(3,s&7,d&7);}} /* and r64,r64 (LL; fix 2026-08-05) */
     else if(!strcmp(mn,"与")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x21);modrm(3,s&7,d&7);}} /* and r32,r32 */
        else if(!strcmp(mn,"右移")){SK;int r=RG;SK;if(*p==',')p++;SK;if(r>=0){rex(0,0,0,r&8);b(0xD3);modrm(3,7,r&7);}} /* sar r32, cl */
        else if(!strcmp(mn,"算术右移64")){SK;int r=RG;SK;if(*p==',')p++;SK;if(r>=0){rex(1,0,0,r&8);b(0xD3);modrm(3,7,r&7);}} /* sar r64, cl (LL; fix 2026-08-05) */
     else if(!strcmp(mn,"逻辑右移64")){SK;int r=RG;SK;if(*p==',')p++;SK;if(r>=0){rex(1,0,0,r&8);b(0xD3);modrm(3,5,r&7);}} /* shr r64, cl (LL; fix 2026-08-05) */
     else if(!strcmp(mn,"循环左移")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){if(r>=8)b(0x41);else b(0x40);b(0xC1);modrm(3,0,r&7);b(v&0xff);}} /* rol r,imm8 (P0) */
     else if(!strcmp(mn,"循环左移cl")){SK;int r=RG;if(r>=0){if(r>=8)b(0x41);else b(0x40);b(0xD3);modrm(3,0,r&7);}} /* rol r,cl (P0) */
     else if(!strcmp(mn,"循环右移")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){if(r>=8)b(0x41);else b(0x40);b(0xC1);modrm(3,1,r&7);b(v&0xff);}} /* ror r,imm8 (P0) */
     else if(!strcmp(mn,"循环右移cl")){SK;int r=RG;if(r>=0){if(r>=8)b(0x41);else b(0x40);b(0xD3);modrm(3,1,r&7);}} /* ror r,cl (P0) */
     else if(!strcmp(mn,"带进左移")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){if(r>=8)b(0x41);else b(0x40);b(0xC1);modrm(3,2,r&7);b(v&0xff);}} /* rcl r,imm8 (P0) */
     else if(!strcmp(mn,"带进左移cl")){SK;int r=RG;if(r>=0){if(r>=8)b(0x41);else b(0x40);b(0xD3);modrm(3,2,r&7);}} /* rcl r,cl (P0) */
     else if(!strcmp(mn,"带进右移")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){if(r>=8)b(0x41);else b(0x40);b(0xC1);modrm(3,3,r&7);b(v&0xff);}} /* rcr r,imm8 (P0) */
     else if(!strcmp(mn,"带进右移cl")){SK;int r=RG;if(r>=0){if(r>=8)b(0x41);else b(0x40);b(0xD3);modrm(3,3,r&7);}} /* rcr r,cl (P0) */
     else if(!strcmp(mn,"循环左移64")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){rex(1,0,0,r&8);b(0xC1);modrm(3,0,r&7);b(v&0xff);}} /* rol r64,imm8 (P0) */
     else if(!strcmp(mn,"循环左移64cl")){SK;int r=RG;if(r>=0){rex(1,0,0,r&8);b(0xD3);modrm(3,0,r&7);}} /* rol r64,cl (P0) */
     else if(!strcmp(mn,"循环右移64")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){rex(1,0,0,r&8);b(0xC1);modrm(3,1,r&7);b(v&0xff);}} /* ror r64,imm8 (P0) */
     else if(!strcmp(mn,"循环右移64cl")){SK;int r=RG;if(r>=0){rex(1,0,0,r&8);b(0xD3);modrm(3,1,r&7);}} /* ror r64,cl (P0) */
     else if(!strcmp(mn,"带进左移64")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){rex(1,0,0,r&8);b(0xC1);modrm(3,2,r&7);b(v&0xff);}} /* rcl r64,imm8 (P0) */
     else if(!strcmp(mn,"带进左移64cl")){SK;int r=RG;if(r>=0){rex(1,0,0,r&8);b(0xD3);modrm(3,2,r&7);}} /* rcl r64,cl (P0) */
     else if(!strcmp(mn,"带进右移64")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){rex(1,0,0,r&8);b(0xC1);modrm(3,3,r&7);b(v&0xff);}} /* rcr r64,imm8 (P0) */
     else if(!strcmp(mn,"带进右移64cl")){SK;int r=RG;if(r>=0){rex(1,0,0,r&8);b(0xD3);modrm(3,3,r&7);}} /* rcr r64,cl (P0) */
     else if(!strcmp(mn,"算术右移")){SK;int r=RG;SK;if(*p==',')p++;SK;if(r>=0){rex(0,0,0,r&8);b(0xD3);modrm(3,7,r&7);}} /* sar r32, cl (fix 2026-08-05) */
        else if(!strcmp(mn,"逻辑右移")){SK;int r=RG;SK;if(*p==',')p++;SK;if(r>=0){rex(0,0,0,r&8);b(0xD3);modrm(3,5,r&7);}} /* shr r32, cl (fix 2026-08-05: unsigned >>) */
        else if(!strcmp(mn,"乘64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,d&8,0,s&8);b(0x0F);b(0xAF);modrm(3,d&7,s&7);}} /* imul r64,r64 */
        else if(!strcmp(mn,"乘32")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){if(s>=8){b(0x41);}b(0xF7);b(0xE0|(s&7));}} /* mul r32 (eax=eax*src), no REX for low regs */
        else if(!strcmp(mn,"取反")){SK;int r=RG;if(r>=0){if(r>=8){b(0x41);}b(0xF7);b(0xD8|(r&7));}} /* neg r32, no REX for low regs */
        else if(!strcmp(mn,"按位反")){SK;int r=RG;if(r>=0){if(r>=8){b(0x41);}b(0xF7);b(0xD0|(r&7));}} /* not r32, no REX for low regs */
     else if(!strcmp(mn,"自增64")){int r=RG;if(r>=0){rex(1,0,0,r&8);b(0xFF);modrm(3,0,r&7);}} /* inc r64 (P0) */
     else if(!strcmp(mn,"自减64")){int r=RG;if(r>=0){rex(1,0,0,r&8);b(0xFF);modrm(3,1,r&7);}} /* dec r64 (P0) */
     else if(!strcmp(mn,"取反64")){SK;int r=RG;if(r>=0){rex(1,0,0,r&8);b(0xF7);modrm(3,3,r&7);}} /* neg r64 (P0) */
     else if(!strcmp(mn,"乘无64")){SK;int r=RG;if(r>=0){rex(1,0,0,r&8);b(0xF7);modrm(3,4,r&7);}} /* mul r64 (P0) */
     else if(!strcmp(mn,"乘即")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;SK;if(*p==',')p++;int v=NUM;if(d>=0&&s>=0){rex(1,d&8,0,s&8);if(v>=-128&&v<=127){b(0x6B);modrm(3,d&7,s&7);b(v&0xff);}else{b(0x69);modrm(3,d&7,s&7);b4(v);}}} /* imul r64,r64,imm (P0) */
        else if(!strcmp(mn,"加指针")){SK;if(!strncmp(p,"rax",3)){p+=3;SK;if(*p==',')p++;int v=NUM;if(v>=-128&&v<=127){rex(1,0,0,0);b(0x83);modrm(3,0,0);b(v&0xff);}else{rex(1,0,0,0);b(0x05);b4(v);}}} /* add rax,imm */
        else if(!strcmp(mn,"浮取参")){b(0xF2);b(0x41);b(0x0F);b(0x10);b(0x45);b(0x00);} /* movsd xmm0,[r13] */
        else if(!strcmp(mn,"扩展符号")){b(0x99);} /* CDQ (fix 2026-08-06 M1: 原 48 99=CQO 查 RAX 位63，32位负数高32位为0 → 变无符号除法) */
        else if(!strcmp(mn,"减即")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){if(r>=8)b(0x41);else b(0x40);b(0x83);modrm(3,5,r&7);b(v);}} /* sub r,imm8 */
        else if(!strcmp(mn,"运即")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){if(r>=8)b(0x41);else b(0x40);if(v>=-128&&v<=127){b(0x83);modrm(3,0,r&7);b(v&0xff);}else{b(0x81);modrm(3,0,r&7);b4(v);}}}
     else if(!strcmp(mn,"加带进")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x11);modrm(3,s&7,d&7);}} /* adc r32,r32 (P0) */
     else if(!strcmp(mn,"加带进64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x11);modrm(3,s&7,d&7);}} /* adc r64,r64 (P0) */
     else if(!strcmp(mn,"减带借")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x19);modrm(3,s&7,d&7);}} /* sbb r32,r32 (P0) */
     else if(!strcmp(mn,"减带借64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x19);modrm(3,s&7,d&7);}} /* sbb r64,r64 (P0) */
     else if(!strcmp(mn,"加带进即")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){if(r>=8)b(0x41);else b(0x40);if(v>=-128&&v<=127){b(0x83);modrm(3,2,r&7);b(v&0xff);}else{b(0x81);modrm(3,2,r&7);b4(v);}}} /* adc r,imm (P0) */
     else if(!strcmp(mn,"减带借即")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){if(r>=8)b(0x41);else b(0x40);if(v>=-128&&v<=127){b(0x83);modrm(3,3,r&7);b(v&0xff);}else{b(0x81);modrm(3,3,r&7);b4(v);}}} /* sbb r,imm (P0) */
     else if(!strcmp(mn,"加带进即64")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){rex(1,0,0,r&8);if(v>=-128&&v<=127){b(0x83);modrm(3,2,r&7);b(v&0xff);}else{b(0x81);modrm(3,2,r&7);b4(v);}}} /* adc r64,imm (P0) */
     else if(!strcmp(mn,"减带借即64")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){rex(1,0,0,r&8);if(v>=-128&&v<=127){b(0x83);modrm(3,3,r&7);b(v&0xff);}else{b(0x81);modrm(3,3,r&7);b4(v);}}} /* sbb r64,imm (P0) */ /* add r,imm8/imm32 (fix 2026-08-03: handler was MISSING → -S path dropped 运即; imm8-only sign-extended 128→-128) */
        else if(!strcmp(mn,"清零")){SK;int r=RG;if(r==2){b(0x31);b(0xD2);}} /* xor edx,edx */
        else if(!strcmp(mn,"除32")){SK;int r=RG;if(r>=0){if(r>=8)b(0x41);b(0xF7);modrm(3,6,r&7);}} /* div r */
        else if(!strcmp(mn,"除64")){SK;int r=RG;if(r>=0){b(0x48);b(0x99);b(0x48);b(0xF7);modrm(3,7,r&7);}} /* cqo; idiv r64 (LL; fix 2026-08-05) */
     else if(!strcmp(mn,"除余64")){SK;int r=RG;if(r>=0){b(0x48);b(0x99);b(0x48);b(0xF7);modrm(3,7,r&7);}} /* cqo; idiv r64（qcc 文本自带「移64 r0, r2」取余数；fix 2026-08-05） */
     else if(!strcmp(mn,"整除")){SK;int r=RG;if(r>=0){if(r>=8)b(0x41);b(0xF7);modrm(3,7,r&7);}} /* idiv r */
        else if(!strcmp(mn,"无符号除")){SK;int r=RG;if(r>=0){if(r>=8)b(0x41);b(0xF7);modrm(3,6,r&7);}} /* div r (unsigned; fix 2026-08-05: qcc emit_hex_digits emits it for %x, was missing → -S path silently dropped it) */
else if(!strcmp(mn,"无符号除64")){SK;int r=RG;if(r>=0){b(0x48);b(0xF7);modrm(3,6,r&7);}} /* div r64 (unsigned LL; fix 2026-08-06 M1) */
else if(!strcmp(mn,"无符号除余64")){SK;int r=RG;if(r>=0){b(0x48);b(0xF7);modrm(3,6,r&7);}} /* div r64, rax=rdx 取余（qcc 文本自带「移64 r0, r2」; fix 2026-08-06 M1） */
        else if(!strcmp(mn,"加字节")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){b(0x80);modrm(3,0,r&7);b(v);}} /* add r8,imm8 */
        else if(!strcmp(mn,"减字节")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r>=0){b(0x80);modrm(3,5,r&7);b(v);}} /* sub r8,imm8 */
     else if(!strcmp(mn,"双左移")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;SK;if(*p==',')p++;int v=NUM;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0xA4);modrm(3,s&7,d&7);b(v&0xff);}} /* shld r/m32,r32,imm8 (P0) */
     else if(!strcmp(mn,"双左移64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;SK;if(*p==',')p++;int v=NUM;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x0F);b(0xA4);modrm(3,s&7,d&7);b(v&0xff);}} /* shld r/m64,r64,imm8 (P0) */
     else if(!strcmp(mn,"双右移")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;SK;if(*p==',')p++;int v=NUM;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0xAC);modrm(3,s&7,d&7);b(v&0xff);}} /* shrd r/m32,r32,imm8 (P0) */
     else if(!strcmp(mn,"双右移64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;SK;if(*p==',')p++;int v=NUM;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x0F);b(0xAC);modrm(3,s&7,d&7);b(v&0xff);}} /* shrd r/m64,r64,imm8 (P0) */
        else if(!strcmp(mn,"比较字节即")){SK;int r=RG;SK;if(*p==',')p++;int v=NUM;if(r==0){b(0x3C);b(v);}else if(r>=0){b(0x80);modrm(3,7,r&7);b(v);}} /* cmp al,imm8 short form */
        else if(!strcmp(mn,"减64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x29);modrm(3,s&7,d&7);}} /* sub r64,r64 */
        else if(!strcmp(mn,"比较64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x39);modrm(3,s&7,d&7);}} /* cmp r64,r64 */
        else if(!strcmp(mn,"比较无")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){b(0x39);modrm(3,s&7,d&7);}} /* cmp r32,r32 NO REX */
        else if(!strcmp(mn,"取浮标")){SK;if(!strncmp(p,"r0",2)){p+=2;SK;if(*p==',')p++;SK;b(0x66);b(0x48);b(0x0F);b(0x7E);b(0xC0);}} /* movq rax,xmm0 */
        else if(!strcmp(mn,"测试位")){SK;if(!strncmp(p,"r0",2)){p+=2;SK;if(*p==',')p++;int v=NUM;rex(1,0,0,0);b(0x0F);b(0xBA);b(0xE0);b(v);}} /* bt rax,imm8 */
     else if(!strcmp(mn,"测试位64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x0F);b(0xA3);modrm(3,s&7,d&7);}} /* bt r64,r64 (P0) */
     else if(!strcmp(mn,"位测置")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0xAB);modrm(3,s&7,d&7);}} /* bts r32,r32 (P0) */
     else if(!strcmp(mn,"位测置64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x0F);b(0xAB);modrm(3,s&7,d&7);}} /* bts r64,r64 (P0) */
     else if(!strcmp(mn,"位测清")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0xB3);modrm(3,s&7,d&7);}} /* btr r32,r32 (P0) */
     else if(!strcmp(mn,"位测清64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x0F);b(0xB3);modrm(3,s&7,d&7);}} /* btr r64,r64 (P0) */
     else if(!strcmp(mn,"位测翻")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0xBB);modrm(3,s&7,d&7);}} /* btc r32,r32 (P0) */
     else if(!strcmp(mn,"位测翻64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x0F);b(0xBB);modrm(3,s&7,d&7);}} /* btc r64,r64 (P0) */
     else if(!strcmp(mn,"扫零位")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0xBC);modrm(3,d&7,s&7);}} /* bsf r32,r/m32 (P0) */
     else if(!strcmp(mn,"扫零位64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x0F);b(0xBC);modrm(3,d&7,s&7);}} /* bsf r64,r/m64 (P0) */
     else if(!strcmp(mn,"扫置位")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0xBD);modrm(3,d&7,s&7);}} /* bsr r32,r/m32 (P0) */
     else if(!strcmp(mn,"扫置位64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x0F);b(0xBD);modrm(3,d&7,s&7);}} /* bsr r64,r/m64 (P0) */
        else if(!strcmp(mn,"存零")){SK;if(*p=='[')p++;int m=RG;SK;if(*p==']')p++;SK;if(*p==',')p++;SK;int r=RG;if(r>=0&&m>=0){b(0x89);modrm(0,r&7,m&7);}} /* mov [m],r32 NO REX */
        else if(!strcmp(mn,"存浮")){SK;if(*p=='[')p++;int m=RG;SK;if(*p==']')p++;SK;if(*p==',')p++;SK;if(m==3){b(0xF2);b(0x0F);b(0x11);modrm(0,0,3);}} /* movsd [rbx],xmm0 */
        else if(!strcmp(mn,"存字节")){SK;if(*p=='[')p++;int m=RG;SK;if(*p==']')p++;SK;if(*p==',')p++;SK;if(m==3){b(0x40);b(0x88);modrm(0,0,3);}} /* mov [rbx],al */
        else if(!strcmp(mn,"存64")){SK;if(*p=='[')p++;int m=RG;SK;if(*p==']')p++;SK;if(*p==',')p++;SK;int v=RG;if(m==0&&v==3){b(0x48);b(0x89);modrm(0,3,0);}else if(m==3){b(0x48);b(0x89);modrm(0,0,3);}} /* mov [rax],rbx / mov [rbx],rax (fix 2026-08-03: case-10 64-bit element store was bare) */
                else if(!strcmp(mn,"存指64")){SK;if(*p=='[')p++;int m=RG;SK;int sg=1;if(*p=='+')p++;else if(*p=='-'){sg=-1;p++;}int ov=NUM*sg;SK;if(*p==']')p++;SK;if(*p==',')p++;SK;int rr=RG;if(m==1&&rr==0){b(0x48);b(0x89);modrm(1,0,1);b(ov);}} /* mov [rcx+disp8],rax (sret copy text fix 2026-08-03) */
        else if(!strcmp(mn,"存指32")){SK;if(*p=='[')p++;int m=RG;SK;int sg=1;if(*p=='+')p++;else if(*p=='-'){sg=-1;p++;}int ov=NUM*sg;SK;if(*p==']')p++;SK;if(*p==',')p++;SK;int rr=RG;if(m==1&&rr==0){b(0x89);modrm(1,0,1);b(ov);}} /* mov [rcx+disp8],eax */
        else if(!strcmp(mn,"存指16")){SK;if(*p=='[')p++;int m=RG;SK;int sg=1;if(*p=='+')p++;else if(*p=='-'){sg=-1;p++;}int ov=NUM*sg;SK;if(*p==']')p++;SK;if(*p==',')p++;SK;int rr=RG;if(m==1&&rr==0){b(0x66);b(0x89);modrm(1,0,1);b(ov);}} /* mov [rcx+disp8],ax */
        else if(!strcmp(mn,"存指8")){SK;if(*p=='[')p++;int m=RG;SK;int sg=1;if(*p=='+')p++;else if(*p=='-'){sg=-1;p++;}int ov=NUM*sg;SK;if(*p==']')p++;SK;if(*p==',')p++;SK;int rr=RG;if(m==1&&rr==0){b(0x88);modrm(1,0,1);b(ov);}} /* mov [rcx+disp8],al */
else if(!strcmp(mn,"存32")){SK;if(*p=='[')p++;int m=RG;SK;if(*p==']')p++;SK;if(*p==',')p++;SK;if(m==3){b(0x40);b(0x89);modrm(0,0,3);}} /* mov [rbx],eax */
        else if(!strcmp(mn,"非负跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x89),b4(0);} /* jns rel32 */
     else if(!strcmp(mn,"负跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x88),b4(0);} /* js rel32 (P0) */
     else if(!strcmp(mn,"大跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x8F),b4(0);} /* jg rel32 (P0) */
     else if(!strcmp(mn,"小跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x8C),b4(0);} /* jl rel32 (P0) */
     else if(!strcmp(mn,"溢跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x80),b4(0);} /* jo rel32 (P0) */
     else if(!strcmp(mn,"不溢跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x81),b4(0);} /* jno rel32 (P0) */
     else if(!strcmp(mn,"奇跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x8A),b4(0);} /* jp rel32 (P0) */
     else if(!strcmp(mn,"偶跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x8B),b4(0);} /* jnp rel32 (P0) */
        else if(!strcmp(mn,"大于等跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x8D),b4(0);} /* jge rel32 */
        else if(!strcmp(mn,"小于等跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x8E),b4(0);} /* jle rel32 */
        else if(!strcmp(mn,"高于等跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x83),b4(0);} /* jae rel32 */
        else if(!strcmp(mn,"低于跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x82),b4(0);} /* jb rel32 */
        else if(!strcmp(mn,"高于跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),b(0x0F),b(0x87),b4(0);} /* ja rel32 */
        else if(!strcmp(mn,"零扩展空")){SK;if(!strncmp(p,"eax",3)){b(0x40);b(0x0F);b(0xB6);b(0xC0);}else{b(0x40);b(0x0F);b(0xB6);b(0xD2);}} /* movzx eax,al / edx,dl WITH empty REX */
        else if(!strcmp(mn,"减无")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){b(0x29);modrm(3,s&7,d&7);}} /* sub WITHOUT REX */
        else if(!strcmp(mn,"加")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0)alu_rr(5,d,s);}
        else if(!strcmp(mn,"或64")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x09);modrm(3,s&7,d&7);}} /* or r64,r64 (LL; fix 2026-08-05) */
     else if(!strcmp(mn,"异或64")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x31);modrm(3,s&7,d&7);}} /* xor r64,r64 (LL; fix 2026-08-05) */
     else if(!strcmp(mn,"或")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x09);modrm(3,s&7,d&7);}} /* or r32,r32 */
        else if(!strcmp(mn,"异或")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x31);modrm(3,s&7,d&7);}} /* xor r32,r32 (fix 2026-08-03: mnemonic existed in qcc text, asm_zh silently skipped it) */
        else if(!strcmp(mn,"左移64")){SK;int r=RG;SK;if(*p==',')p++;SK;if(r>=0){rex(1,0,0,r&8);b(0xD3);modrm(3,4,r&7);}} /* shl r64, cl (LL; fix 2026-08-05) */
     else if(!strcmp(mn,"左移")){SK;int r=RG;SK;if(*p==',')p++;SK;if(!strncmp(p,"cl",2)){if(r>=0){rex(0,0,0,r&8);b(0xD3);modrm(3,4,r&7);}}else{int v=NUM;if(r>=0){rex(0,0,0,r&8);b(0xC1);modrm(3,4,r&7);b(v);}}}
        else if(!strcmp(mn,"加64")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x01);modrm(3,s&7,d&7);}} /* add r64,r64 (LL array idx + LL add; fix 2026-08-05) */
        else if(!strcmp(mn,"减")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0)alu_rr(6,d,s);}
        else if(!strcmp(mn,"乘")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0)alu_rr(7,d,s);}
        else if(!strcmp(mn,"比较")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0)alu_rr(8,d,s);}
        else if(!strcmp(mn,"测试")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0)test_rr(d,s);} else if(!strcmp(mn,"测试64")){SK;int d=RG;SK;if(*p==',')p++;SK;int s=RG;if(d>=0&&s>=0){rex(1,s&8,0,d&8);b(0x85);modrm(3,s&7,d&7);}} /* test r64,r64 (fix 2026-08-06 %lld) */
        else if(!strcmp(mn,"测试al")){b(0x84);b(0xC0);}
        else if(!strcmp(mn,"置等")){setcc(8);}
        else if(!strcmp(mn,"置不等")){setcc(10);}
        else if(!strcmp(mn,"置低")){setcc(31);} /* setb (double <) */
        else if(!strcmp(mn,"置高")){setcc(32);} /* seta (double >) */
        else if(!strcmp(mn,"置低等于")){setcc(22);} /* setle (LL <; fix 2026-08-05) */
     else if(!strcmp(mn,"置高等于")){setcc(23);} /* setge (LL >=; fix 2026-08-05) */
     else if(!strcmp(mn,"置低等")){setcc(33);} /* setbe (double <=) */
        else if(!strcmp(mn,"置高等")){setcc(34);} /* setae (double >=) */
        else if(!strcmp(mn,"置小")){setcc(11);}
        else if(!strcmp(mn,"置大")){setcc(21);}
        else if(!strcmp(mn,"置小等")){setcc(22);}
        else if(!strcmp(mn,"置大等")){setcc(23);}
        else if(!strcmp(mn,"置条件")){setcc(8);}
        else if(!strcmp(mn,"取符号字节")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0xBE);modrm(3,d&7,s&7);}} /* movsx r32,r/m8 (P0) */
     else if(!strcmp(mn,"取符号字")){SK;int d=RG;SK;if(*p==',')p++;int s=RG;if(d>=0&&s>=0){rex(0,s&8,0,d&8);b(0x0F);b(0xBF);modrm(3,d&7,s&7);}} /* movsx r32,r/m16 (P0) */
     else if(!strcmp(mn,"零扩展")){SK;if(!strncmp(p,"SIB",3)){b(0x43);b(0x0F);b(0xB6);b(0x04);b(0x1A);}else{int dst=-1;if(!strncmp(p,"eax",3)){dst=0;p+=3;}else if(!strncmp(p,"ecx",3)){dst=1;p+=3;}else if(!strncmp(p,"edx",3)){dst=2;p+=3;}if(dst>=0){SK;if(*p==',')p++;SK;if(*p=='['){p++;int br=RG;SK;if(br>=8){b(0x41);}b(0x0F);b(0xB6);modrm(0,dst,br&7);}else{if(dst==0)movzx_eax_al();else if(dst==1){b(0x0F);b(0xB6);modrm(3,1,1);}else{b(0x0F);b(0xB6);modrm(3,2,2);}}}else movzx_eax_al();}}

        else if(!strcmp(mn,"零扩展字")){SK;if(!strncmp(p,"r0",2))p+=2;SK;if(*p==',')p++;int v=MEM;b(0x0F);b(0xB7);b(0x85);b4(v);} /* movzx eax,word[rbp+disp32] */
        else if(!strcmp(mn,"零扩展字节")){SK;if(!strncmp(p,"r0",2))p+=2;SK;if(*p==',')p++;int v=MEM;b(0x0F);b(0xB6);b(0x85);b4(v);} /* movzx eax,byte[rbp+disp32] */
        else if(!strcmp(mn,"存字节帧")){b(0x41);b(0x88);modrm(1,1,5);b(0);} /* mov byte[r13+0],cl */
        else if(!strcmp(mn,"存字节0")){b(0x41);b(0xC6);modrm(1,0,5);b(0);b(0);} /* mov byte[r13+0],0 */
        else if(!strcmp(mn,"存字节0r12")){b(0x41);b(0xC6);b(0x04);b(0x24);b(0);} /* mov byte[r12],0 (sprintf NUL — fix 2026-08-03) */
        else if(!strcmp(mn,"自增")){int r=RG;if(r>=8)b(0x41);if(r>=0){b(0xFF);modrm(3,0,r&7);}} /* inc r8-r15 */
        else if(!strcmp(mn,"存32rax")){b(0x40);b(0x89);b(0x18);} /* MOV [rax],ebx (32-bit array write) */
        else if(!strcmp(mn,"存字节rax")){b(0x40);b(0x88);b(0x18);} /* MOV [rax],bl (char array write) */
        else if(!strcmp(mn,"存栈索引")){b(0x4F);b(0x89);b(0x04);b(0xF4);} /* mov [r12+r14*8],r8 */
        else if(!strcmp(mn,"跳转")){int li=LR;if(li>=0)patch_label(cp+1,li,2),jmp_rel(0);}
     else if(!strcmp(mn,"跳转短")){int li=LR;if(li>=0)patch_label(cp+1,li,3),b(0xEB),b(0);} /* jmp rel8 (P0) */
     else if(!strcmp(mn,"环跳")){int li=LR;if(li>=0)patch_label(cp+1,li,3),b(0xE2),b(0);} /* loop rel8 (P0) */
     else if(!strcmp(mn,"环等跳")){int li=LR;if(li>=0)patch_label(cp+1,li,3),b(0xE1),b(0);} /* loope rel8 (P0) */
     else if(!strcmp(mn,"环不等跳")){int li=LR;if(li>=0)patch_label(cp+1,li,3),b(0xE0),b(0);} /* loopne rel8 (P0) */
     else if(!strcmp(mn,"间跳")){SK;int r=RG;if(r>=0){rex(1,0,0,r&8);b(0xFF);modrm(3,4,r&7);}} /* jmp r64 (P0) */
     else if(!strcmp(mn,"无操作")){b(0x90);} /* nop (P0) */
     else if(!strcmp(mn,"无操作3")){b(0x0F);b(0x1F);b(0x00);} /* 3-byte nop (P1) */
     else if(!strcmp(mn,"无操作4")){b(0x0F);b(0x1F);b(0x40);b(0x00);} /* 4-byte nop (P1) */
     else if(!strcmp(mn,"无操作9")){b(0x0F);b(0x1F);b(0x84);b(0x00);b(0x00);b(0x00);b(0x00);b(0x00);b(0x00);} /* 9-byte nop (P1) */
        else if(!strcmp(mn,"为零跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),jz_rel(0);}
        else if(!strcmp(mn,"非零跳")){int li=LR;if(li>=0)patch_label(cp+2,li,1),jnz_rel(0);}
        else if(!strcmp(mn,"调用")){int li=LR;if(li>=0)patch_label(cp+1,li,0),call_rel(0);}
        else if(!strcmp(mn,"调")){SK;if(!strncmp(p,"r10",3)){p+=3;b(0x49);b(0xFF);b(0xD2);}} /* call r10 (fnptr indirect call — fix 2026-08-03: text had no handler, H2 silently dropped the call) */
        else if(!strcmp(mn,"减栈")){int v=NUM;sub_rsp_imm(v);}
        else if(!strcmp(mn,"加栈")){int v=NUM;add_rsp_imm(v);}
        else if(!strcmp(mn,"对齐栈")){b(0x48);b(0x83);b(0xE4);b(0xF0);} /* and rsp,-16 (Win64 ABI) */
        else if(!strcmp(mn,"调系统")){int s=NUM;call_iat(s);}
     else if(!strcmp(mn,"清进位")){b(0xF8);} /* clc (P1) */
     else if(!strcmp(mn,"置进位")){b(0xF9);} /* stc (P1) */
     else if(!strcmp(mn,"翻进位")){b(0xF5);} /* cmc (P1) */
     else if(!strcmp(mn,"清方向")){b(0xFC);} /* cld (P1) */
     else if(!strcmp(mn,"置方向")){b(0xFD);} /* std (P1) */
     else if(!strcmp(mn,"取标志")){b(0x9F);} /* lahf (P1) */
     else if(!strcmp(mn,"存标志")){b(0x9E);} /* sahf (P1) */
     else if(!strcmp(mn,"压标志")){rex(1,0,0,0);b(0x9C);} /* pushfq (P1) */
     else if(!strcmp(mn,"弹标志")){rex(1,0,0,0);b(0x9D);} /* popfq (P1) */
     else if(!strcmp(mn,"中断")){SK;int v=NUM;b(0xCD);b(v&0xff);} /* int imm8 (P1) */
     else if(!strcmp(mn,"中断返")){rex(1,0,0,0);b(0xCF);} /* iretq (P1) */
     else if(!strcmp(mn,"系统调用")){b(0x0F);b(0x05);} /* syscall (P1) */
     else if(!strcmp(mn,"取Cpu")){b(0x0F);b(0xA2);} /* cpuid (P1) */
     else if(!strcmp(mn,"取时钟")){b(0x0F);b(0x31);} /* rdtsc (P1) */
        else if(!strcmp(mn,"取参")){mov_eax_mr13();}
        else if(!strcmp(mn,"取参64")){mov_rax_mr13();}
        else if(!strcmp(mn,"写字节")){SK;if(*p=='['){p++;int br=RG;SK;if(*p==']')p++;SK;if(*p==',')p++;SK;int vr=RG;if(br==8&&vr==0){b(0x41);b(0x88);modrm(0,0,0);}else if(br==9&&vr==0){b(0x41);b(0x88);modrm(0,0,1);}else if(br==8&&vr==9){b(0x45);b(0x88);modrm(0,1,0);} /* mov byte[r8],r9b */else if(br==9&&vr==2){b(0x41);b(0x88);modrm(0,2,1);} /* mov [r9],dl */else mov_r12_cl();}else mov_r12_cl();}
        else if(!strcmp(mn,"读字节")){SK;int vr=RG;SK;if(*p==',')p++;SK;if(*p=='[')p++;int br=RG;if(vr==0&&br==8){b(0x41);b(0x8A);modrm(0,0,0);}else if(vr==2&&br==9){b(0x41);b(0x8A);modrm(0,2,1);}else if(vr==0&&br==9){b(0x41);b(0x8A);modrm(0,0,1);}}
        else if(!strcmp(mn,"比较字节")){b(0x38);modrm(3,2,0);}
        else if(!strcmp(mn,"自减")){int r=RG;if(r>=8)b(0x41);if(r>=0){b(0xFF);modrm(3,1,r&7);}}
        else if(!strcmp(mn,"写字符")){mov_r12_al();}
        else if(!strcmp(mn,"取址")){SK;int r=RG;if(r>=0){if(*p==','||*p=='[')p++;int v=MEM;lea_r_mrsp(r,v);}}
        else if(!strcmp(mn,"取帧址")){SK;int r=RG;if(r>=0){if(*p==','||*p=='[')p++;int v=MEM;lea_r_mbrp(r,v);}}
        else if(!strcmp(mn,"取静址")){SK;if(*p=='r')p+=3;SK;if(*p==',')p++;int v=RIPS;lea_rax_rip(v);}
        else if(!strcmp(mn,"取静32")){SK;if(!strncmp(p,"eax",3))p+=3;SK;if(*p==',')p++;int v=RIPS;mov_eax_rip(v);}
        else if(!strcmp(mn,"取静64")){SK;if(!strncmp(p,"rax",3))p+=3;SK;if(*p==',')p++;int v=RIPS;mov_rax_rip64(v);}
        else if(!strcmp(mn,"存静64")){int v=RIPS;SK;if(*p==',')p++;mov_rip_rax64(v);}
        else if(!strcmp(mn,"存静32")){int v=RIPS;SK;if(*p==',')p++;mov_rip_eax(v);}
     else if(!strcmp(mn,"存静字节")){int v=RIPS;SK;if(*p==',')p++;int im=NUM;b(0xC6);b(0x05);b4(v);b(im&0xFF);} /* mov byte [rip+disp], imm8 (fix 2026-08-05) */
        else if(!strcmp(mn,"取值")){SK;int r=RG;SK;if(*p==',')p++;SK;if(*p=='[')p++;int m=RG;if(r>=0&&m>=0)mov_reg_mreg(r,m);}
        else if(!strcmp(mn,"取64")){SK;int r=RG;SK;if(*p==',')p++;SK;if(*p=='['){p++;if(!strncmp(p,"r13",3)){p+=3;SK;if(*p=='+')p++;int dv=NUM;if(r>=0){b(0x49);b(0x8B);b(0x45);b(dv);}SK;if(*p==']')p++;}else if(!strncmp(p,"r10",3)){p+=3;SK;if(*p=='+')p++;int dv=NUM;if(r>=0){b(0x49);b(0x8B);b(0x42);b(dv);}SK;if(*p==']')p++;}else{int m=RG;if(r>=0&&m>=0)mov_reg_mreg64(r,m);}}else{int m=RG;if(r>=0&&m>=0)mov_reg_mreg64(r,m);}}
        else if(!strcmp(mn,"存值")){SK;if(*p=='[')p++;int m=RG;SK;if(*p==',')p++;int r=RG;if(r>=0&&m>=0)mov_mreg_reg(m,r);}
        else if(!strcmp(mn,"存帧64")){int v=MEM;SK;if(*p==',')p++;int r=RG;mov_mbrp_reg64(v,r);}
        else if(!strcmp(mn,"取帧64")){SK;int r=RG;SK;if(*p==',')p++;int v=MEM;if(r>=0)mov_reg_mbrp64(r,v);}
        else if(!strcmp(mn,"存帧32")){int v=MEM;SK;if(*p==',')p++;int r=RG;mov_mbrp_reg(v,r);}
        else if(!strcmp(mn,"取帧32")){SK;int r=RG;SK;if(*p==',')p++;int v=MEM;if(r>=0)mov_reg_mbrp(r,v);}
        else if(!strcmp(mn,"存栈64")){int v=MEM;SK;if(*p==',')p++;int r=RG;mov_mrsp_reg64(v,r);}
        else if(!strcmp(mn,"取栈64")){SK;int r=RG;SK;if(*p==',')p++;int v=MEM;if(r>=0)mov_reg_mrsp64(r,v);}
        /* SSE/Float */
        else if(!strcmp(mn,"浮取栈")){SK;int rg=0;if(!strncmp(p,"xmm1",4)){rg=1;p+=4;}else if(!strncmp(p,"xmm0",4)){p+=4;}SK;if(*p==',')p++;SK;int v=MEM;b(0xF2);b(0x0F);b(0x10);if(v<128&&v>=-128){modrm(1,rg,4);b(0x24);b(v);}else{modrm(2,rg,4);b(0x24);b4(v);}} /* movsd xmm0,[rsp+disp] */
        else if(!strcmp(mn,"浮存栈")){int v=MEM;SK;if(*p==',')p++;SK;int rg=0;if(!strncmp(p,"xmm1",4)){rg=1;p+=4;}else if(!strncmp(p,"xmm0",4)){p+=4;}b(0xF2);b(0x0F);b(0x11);if(v<128&&v>=-128){modrm(1,rg,4);b(0x24);b(v);}else{modrm(2,rg,4);b(0x24);b4(v);}} /* movsd [rsp+disp],xmm0 */
        else if(!strcmp(mn,"浮取静")){SK;if(!strncmp(p,"xmm0",4))p+=4;SK;if(*p==',')p++;SK;if(!strncmp(p,"[rip+DBL",8)){p+=8;int di=atoi(p);if(dpn2<2048){b(0xF2);b(0x0F);b(0x10);b(0x05);b4(0);dbl_patches2[dpn2].at=cp-4;dbl_patches2[dpn2].idx=di;dpn2++;}}else{int v=RIPS;b(0xF2);b(0x0F);b(0x10);b(0x05);b4(v);}} /* movsd xmm0,[rip+disp] */
        else if(!strcmp(mn,"浮存静")){int v=RIPS;SK;if(*p==',')p++;b(0xF2);b(0x0F);b(0x11);b(0x05);b4(v);} /* movsd [rip+disp],xmm0 */
        else if(!strcmp(mn,"浮取帧")){SK;if(!strncmp(p,"xmm1",4)){p+=4;SK;if(*p==',')p++;int v=MEM;movsd_xmm1_mbrp(v);}else{if(!strncmp(p,"xmm0",4))p+=4;SK;if(*p==',')p++;int v=MEM;movsd_xmm0_mbrp(v);}}
        else if(!strcmp(mn,"浮存帧")){int v=MEM;SK;if(*p==',')p++;SK;int rg=0;if(!strncmp(p,"xmm1",4)){rg=1;p+=4;}else if(!strncmp(p,"xmm2",4)){rg=2;p+=4;}else if(!strncmp(p,"xmm3",4)){rg=3;p+=4;}else if(!strncmp(p,"xmm0",4)){p+=4;}movsd_mbrp_xmmreg(v,rg);}
        else if(!strcmp(mn,"浮移")){SK;if(!strncmp(p,"xmm1",4)){p+=4;SK;if(*p==',')p++;SK;movsd_xmm1_xmm0();}else if(!strncmp(p,"xmm0",4)){p+=4;SK;if(*p==',')p++;SK;if(!strncmp(p,"xmm1",4)){p+=4;movsd_xmm0_xmm1();}else movsd_xmm1_xmm0();}}
        else if(!strcmp(mn,"浮加")){addsd_xmm0_xmm1();}
        else if(!strcmp(mn,"浮减")){subsd_xmm0_xmm1();}
        else if(!strcmp(mn,"浮乘")){mulsd_xmm0_xmm1();}
        else if(!strcmp(mn,"浮除")){divsd_xmm0_xmm1();}
        else if(!strcmp(mn,"整转浮")){SK;if(!strncmp(p,"xmm1",4)){p+=4;SK;if(*p==',')p++;SK;cvtsi2sd_xmm1_eax();}else{if(!strncmp(p,"xmm0",4))p+=4;SK;if(*p==',')p++;SK;cvtsi2sd_xmm0_eax();}}
        else if(!strcmp(mn,"浮转整")){cvttsd2si_eax_xmm0();}
        else if(!strcmp(mn,"浮比较")){comisd_xmm0_xmm1();}
        else if(!strcmp(mn,"压浮")){push_xmm0();}
        else if(!strcmp(mn,"弹浮")){pop_xmm0();}
        else if(!strcmp(mn,".堆计")){int v=NUM;if(v>0)asm_stc_n=v;}
        else if(!strcmp(mn,".布局")){SK;if(!strncmp(p,"code_end=",9)){p+=9;exp_code_end=NUM;}SK;if(!strncmp(p,"data_base=0x",12)||!strncmp(p,"data_base=0X",12)){p+=12;exp_data_base=(int)strtoul(p,NULL,16);}}
        else if(!strcmp(mn,".字串")){SK;if(*p=='"'){if(str_cnt2<1024)str_offs2[str_cnt2++]=sdp;p++;while(*p&&*p!='"'){if(sdp>=sdc-4){sdc+=256;sdat=realloc(sdat,sdc);}sdat[sdp++]=*p;if(*p=='\\'&&p[1]&&p[1]=='n'){p++;sdp--;sdat[sdp++]=10;}else if(*p=='\\'&&p[1]&&p[1]=='t'){p++;sdp--;sdat[sdp++]=9;}else if(*p=='\\'&&p[1]&&p[1]=='"'){p++;sdp--;sdat[sdp++]=34;}else if(*p=='\\'&&p[1]&&p[1]=='\\'){p++;sdp--;sdat[sdp++]=92;}p++;}sdat[sdp++]=0;if(*p=='"')p++;}}
        else if(!strcmp(mn,".浮点")){while(sdp%8!=0){if(sdp>=sdc-4){sdc+=256;sdat=realloc(sdat,sdc);}sdat[sdp++]=0;}if(dbl_cnt2<1024)dbl_offs2[dbl_cnt2++]=sdp;SK;double v=0;int sign=1;if(*p=='-'){sign=-1;p++;}while(*p>='0'&&*p<='9'){v=v*10+(*p-'0');p++;}if(*p=='.'){p++;double fr=0,sc=1;while(*p>='0'&&*p<='9'){fr=fr*10+(*p-'0');sc*=10;p++;}v+=fr/sc;/* fix 2026-08-06: 整数累加+单次除法，消除 fr*=0.1 累积误差（与 qcc fp_parse 同构）。6.123000 再解析 = ...CB 与直发一致，旧法 ...CA 差 1 ULP */}v*=sign;unsigned char*db=(unsigned char*)&v;for(int k=0;k<8;k++){if(sdp>=sdc-4){sdc+=256;sdat=realloc(sdat,sdc);}sdat[sdp++]=db[k];}}
        else { fprintf(stderr, "[ERR] asm_zh: unknown instruction '%s' at line %d\n", mn, line_no); exit(1); } /* fix 2026-08-05: unknown mnemonics were silently dropped → broken exe */
        while(*p&&*p!='\n'){p++;} if(*p=='\n'){p++;line_no++;}
    }
    resolve_patches();
    /* pad to qcc's exact code_end before resolving data refs (fix H1==H2) */
    if (emit_data && exp_code_end > cp) {
        while (cp + (exp_code_end - cp) + sdp >= 0x400000) {
            unsigned char*nc=realloc(code,0x400000+4096);
            if(!nc){fprintf(stderr,"asm_zh: OOM at pad\n");exit(1);}code=nc;
        }
        while (cp < exp_code_end) code[cp++] = 0x90;
    }
    /* string-address back-patch: VA = ImageBase + text_rva + code_end + str_off
       (matches qcc: 0x400000 + 0x1000 + code_end + off) */
    if (emit_data && spn2 > 0) {
        int code_end = cp;
        for (int i = 0; i < spn2; i++) {
            int off = str_offs2[str_patches2[i].idx];
            int va = 0x400000 + 0x1000 + code_end + off;
            b4_at(str_patches2[i].at, va);
        }
    }
    if (emit_data && fnpn2 > 0) {
        for (int i = 0; i < fnpn2; i++) {
            if (!labels[fn_patches2[i].label].defined) { fprintf(stderr, "[ERR] asm_zh: undefined fn label '%s' at line %d\n", labels[fn_patches2[i].label].name, labels[fn_patches2[i].label].line); exit(1); } /* fix 2026-08-05 */
            int va = 0x400000 + 0x1000 + labels[fn_patches2[i].label].pos;
            b4_at(fn_patches2[i].at, va);
        }
    }
    if (emit_data && dpn2 > 0) {
        int code_end = cp;
        for (int i = 0; i < dpn2; i++) {
            int off = dbl_offs2[dbl_patches2[i].idx];
            int at = dbl_patches2[i].at;
            int disp = code_end + off - at - 4;
            b4_at(at, disp);
        }
    }
    if (sdp>0){while(cp+sdp>=0x400000){unsigned char*nc=realloc(code,0x400000+4096);if(!nc){fprintf(stderr,"asm_zh: OOM at code buffer %d bytes\n",0x400000+4096);exit(1);}code=nc;}memcpy(code+cp,sdat,sdp);cp+=sdp;}
    return 0;
}

int main(int argc, char **argv) {
    const char *inf = NULL, *outf = "a.exe";
    for(int i=1;i<argc;i++){if(!strcmp(argv[i],"-o")&&i+1<argc){outf=argv[++i];}else inf=argv[i];}
    if(!inf){fprintf(stderr,"Usage: asm_zh input.asm [-o out.exe]\n");return 1;}
    FILE *fi=fopen(inf,"rb");if(!fi){fprintf(stderr,"Cannot open %s\n",inf);return 1;}
    fseek(fi,0,SEEK_END);int sz=(int)ftell(fi);fseek(fi,0,SEEK_SET);
    char *src=malloc(sz+1);fread(src,1,sz,fi);src[sz]=0;fclose(fi);
    code=malloc(0x400000);if(!code){fprintf(stderr,"asm_zh: OOM at init\n");return 1;}cp=0;

    /* Pass 1: assemble with provisional base 0x2000, no data emission,
       to measure real code size. IAT/rip offsets are 4-byte fixed width,
       so pure-code cp is base-independent. */
    int r = asm_assemble(src, 0x2000, 0);
    if (r < 0) { fprintf(stderr, "Assembly failed\n"); free(src); free(code); return 1; }
    int code_size = cp;
    /* Free pass-1 sdat, recompute in pass 2 */
    free(sdat);

    /* Real .data base: use qcc's value from .布局 if available, else compute.
       (qcc_x86.c: data_rva_base = (0x1000 + cp + 4095) & ~4095). This is
       the key fix for H1==H2 byte-equivalence (fix 2026-08-03). */
    int real_base = exp_data_base ? exp_data_base : ((0x1000 + code_size + 4095) & ~4095);
    if (real_base < 0x2000) real_base = 0x2000;

    /* Pass 2: assemble with real base + emit data section */
    r = asm_assemble(src, real_base, 1);
    if (r < 0) { fprintf(stderr, "Assembly failed\n"); free(src); free(code); free(sdat); return 1; }

    FILE *fo=fopen(outf,"wb");if(!fo){fprintf(stderr,"Cannot write %s\n",outf);return 1;}
    int entry=-1;for(int i=0;i<ln;i++)if(!strcmp("_入口",labels[i].name)){entry=0x1000+labels[i].pos;break;} /* fix 2026-08-03: was label_pos[labels[i].pos] (never-written array -> entry always 0x1000) */
    if (entry < 0) { fprintf(stderr, "[ERR] asm_zh: no _入口 label\n"); fclose(fo); remove(outf); free(src); free(code); free(sdat); return 1; } /* fix 2026-08-05 */
    if (cp == 0) { fprintf(stderr, "[ERR] asm_zh: empty output\n"); fclose(fo); remove(outf); free(src); free(code); free(sdat); return 1; } /* fix 2026-08-05 */
    write_pe(fo,entry);
    fclose(fo);
    printf("OK: %s (%d bytes code, data_base=0x%X)\n",outf,cp,real_base);
    free(src);free(code);free(sdat);
    return 0;
}
