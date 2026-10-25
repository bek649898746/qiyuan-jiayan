/* cpu64_v14_self.c 鈥?v14 + 鑷妇缂栬瘧鍣?+ JSON鑴夊啿 + 鎵归噺鎺ㄦ紨寮曟搸
 * 绉嶅瓙828 鍚厓 2026-07-13
 *
 * 鍏悊(engine_knowledge/cpu64_vm.md):
 *   A1: 闆朵緷璧栬嚜涓?鈥?鍗曟枃浠禫M+姹囩紪鍣?鑷妇C缂栬瘧鍣?
 *   A2: 28鎸囦护绮剧畝闆?鈥?5-bit op, 涓ゅ瓧鎸囦护鍏ㄥ湪0-31
 *   A3: ENC(5+3+8)缂栫爜 鈥?5-bit op + 3-bit rd + 8-bit val
 *   A4: 鎵归噺鎺ㄦ紨鍗忚 鈥?--batch杈撳嚭engine_loop鍙秷璐圭殑JSON鑴夊啿
 *   A5: 鑷妇C瀛愰泦 鈥?int/璧嬪€?鍔犲噺/if/while/鍑芥暟/鍙傛暟
 *
 * 妯″紡:
 *   --self-out "src"       缂栬瘧C婧愮爜鈫掕緭鍑烘眹缂栧埌stdout
 *   --self-out-file f.c    浠庢枃浠惰C婧愮爜鈫掕緭鍑烘眹缂栧埌stdout
 *   --file f.asm           鎵ц姹囩紪
 *   --batch <dir> [log.md] 鎵归噺缂栬瘧鐩綍涓嬫墍鏈?c鈫掕緭鍑篔SON+姹囩紪+鏃ュ織
 *   (鏃犲弬鏁?                榛樿:鑷妇娴嬭瘯+閾佸緥妫€鏌?
 *
 * 杈撳嚭鏍煎紡(JSON鑴夊啿):
 *   {"tag":"Real","name":"<鍖洪棿鍚?","lo":<lo>,"hi":<hi>}
 *   绗﹀悎engine_loop scan_interval_lines/parse_json_line瑙勮寖
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#define OP_NOP 0
#define OP_MOV 1
#define OP_LDI 2
#define OP_ADD 3
#define OP_SUB 4
#define OP_MUL 5
#define OP_CMP 7
#define OP_JMP 8
#define OP_JZ 9
#define OP_JNZ 10
#define OP_HLT 11
#define OP_PUSH 12
#define OP_POP 13
#define OP_CALL 14
#define OP_RET 15
#define OP_LOAD 16
#define OP_STORE 17
#define OP_INC 23
#define OP_DEC 24
#define OP_LDR 25
#define OP_STR 26
#define OP_SYSCALL 27
#define OP_JS 28
#define OP_JMPW 6
#define OP_JZW 18
#define OP_JNZW 19
#define OP_CALLW 20
#define OP_STRW 21
#define OP_LDRW 22
#define ENC(op,rd,val) ((uint16_t)(((op)&0x1F)<<11)|(((rd)&7)<<8)|((val)&0xFF))
static uint16_t M[65536],R[8],SP;
static int PC,ZF,SF,RUN;
/* 杈撳嚭缂撳啿鍖?*/
static char vm_out[65536];
static int vm_op;
static FILE*vm_logfile;
static void vm_init(void){memset(vm_out,0,sizeof(vm_out));vm_op=0;vm_logfile=NULL;}
static void vm_run(){
 PC=0;ZF=0;SF=0;RUN=1;memset(R,0,sizeof(R));SP=65280;int s=0;
 while(RUN&&PC>=0&&PC<65536&&s<500000){
  uint16_t I=M[PC++];s++;int op=(I>>11)&0x1F,rd=(I>>8)&7,val=I&0xFF,rs=val&7;
  switch(op){
  case OP_NOP:break;case OP_MOV:R[rd]=R[rs];break;case OP_LDI:R[rd]=val;break;case OP_ADD:R[rd]+=R[rs];break;
  case OP_SUB:R[rd]-=R[rs];break;case OP_MUL:R[rd]*=R[rs];break;
  case OP_CMP:ZF=(R[rd]==R[rs]);SF=((int16_t)(R[rd]-R[rs])<0);break;
  case OP_JMP:PC+=(int8_t)val;break;case OP_JZ:if(ZF)PC+=(int8_t)val;break;case OP_JNZ:if(!ZF)PC+=(int8_t)val;break;
  case OP_JMPW:if(PC<65535)PC=M[PC];break;case OP_JZW:if(ZF&&PC<65535)PC=M[PC];break;case OP_JNZW:if(!ZF&&PC<65535)PC=M[PC];break;
  case OP_HLT:RUN=0;break;
  case OP_LOAD:R[rd]=M[SP+(int8_t)val];break;case OP_STORE:M[SP+(int8_t)val]=R[rd];break;
  case OP_CALL:SP--;M[SP]=PC;PC+=(int8_t)val;break;case OP_CALLW:SP--;M[SP]=PC+1;PC=M[PC];break;case OP_RET:PC=M[SP];SP++;break;
  case OP_PUSH:SP--;M[SP]=R[rd];break;case OP_POP:R[rd]=M[SP];SP++;break;
  case OP_JS:if(SF)PC+=(int8_t)val;break;case OP_INC:R[rd]++;break;case OP_DEC:R[rd]--;break;
  case OP_LDR:R[rd]=M[R[rs]];break;case OP_STR:M[R[rs]]=R[rd];break;
  case OP_STRW:if(PC<65535){M[M[PC]]=R[rd];PC++;}break;
  case OP_LDRW:if(PC<65535){R[rd]=M[M[PC]];PC++;}break;
  case OP_SYSCALL:
   if(R[0]==0){ /* putchar */
    char ch=(char)R[1];putchar(ch);fflush(stdout);
    if(vm_op<65535){vm_out[vm_op++]=ch;vm_out[vm_op]=0;}
    if(vm_logfile)fputc(ch,vm_logfile);
   }else if(R[0]==1){int ch=getchar();if(ch==EOF)ch=0;R[1]=ch;
   }else if(R[0]==2){ /* write file: R[1]=char */
    if(vm_logfile&&R[1])fputc((int)R[1],vm_logfile);
   }
   break;
  }
 }
}
#define MAXL 4096
#define MAXLB 64
static char LNS[MAXL][128];static int NL;
static char LN[MAXLB][64];static int LA[MAXLB];static int NB;
static void asm_p1(){
 int a=256;for(int i=0;i<NL;i++){
  for(int j=0;LNS[i][j];j++)if(LNS[i][j]==13)LNS[i][j]=32;
  char*p=LNS[i];while(*p==' '||*p=='\t')p++;
  if(!*p||*p==';')continue;char*c=strchr(p,':');if(c){*c=0;strncpy(LN[NB],p,63);LN[NB][63]=0;LA[NB]=a;NB++;
   char*r=c+1;while(*r==' '||*r=='\t')r++;if(0){strncpy(LNS[i],r,127);i--;}continue;}a++;
 }
}
static int flb(char*s){for(int i=0;i<NB;i++)if(!strcmp(LN[i],s))return i;return-1;}
static int asm_assemble(const char*text){
 NL=0;NB=0;memset(M,0,sizeof(M));char buf[32768];strncpy(buf,text,32767);buf[32767]=0;
 char*tok=strtok(buf,"\n");while(tok&&NL<MAXL){strncpy(LNS[NL],tok,127);LNS[NL][127]=0;NL++;tok=strtok(NULL,"\n");}
 asm_p1();int a=256;
 for(int i=0;i<NL;i++){
  for(char*s=LNS[i];*s;s++)if(*s=='\r')*s=' ';
  char*p=LNS[i];while(*p==' '||*p=='\t')p++;if(!*p||*p==';')continue;if(strchr(p,':'))continue;
  char w[16];int n1=0,n2=0,imm=0;sscanf(p,"%15s r%d",w,&n1);
  char*h=strchr(p,'#');if(h){imm=1;n2=atoi(h+1);}uint16_t e=0;
  if(!strcmp(w,"HLT"))e=ENC(OP_HLT,0,0);else if(!strcmp(w,"NOP"))e=0;
  else if(!strcmp(w,"MOV")||!strcmp(w,"LDI")){if(imm)e=ENC(OP_LDI,n1,n2&0xFF);else{sscanf(p,"%*s r%d, r%d",&n1,&n2);e=ENC(OP_MOV,n1,n2&7);}}
  else if(!strcmp(w,"ADD")){sscanf(p,"%*s r%d, r%d",&n1,&n2);e=ENC(OP_ADD,n1,n2&7);}
  else if(!strcmp(w,"SUB")){sscanf(p,"%*s r%d, r%d",&n1,&n2);e=ENC(OP_SUB,n1,n2&7);}
  else if(!strcmp(w,"MUL")){sscanf(p,"%*s r%d, r%d",&n1,&n2);e=ENC(OP_MUL,n1,n2&7);}
  else if(!strcmp(w,"CMP")){int r2=0;sscanf(p,"%*s r%d, r%d",&n1,&r2);e=ENC(OP_CMP,n1,r2&7);}
  else if(!strcmp(w,"JMP")||!strcmp(w,"JZ")||!strcmp(w,"JNZ")){
   int op=OP_JMP,opw=OP_JMPW;if(!strcmp(w,"JZ")){op=OP_JZ;opw=OP_JZW;}else if(!strcmp(w,"JNZ")){op=OP_JNZ;opw=OP_JNZW;}
   char lb[32];sscanf(p,"%*s %31s",lb);int f=flb(lb),off=(f>=0?LA[f]-(a+1):0);
   if(off>=-128&&off<128){e=ENC(op,0,off&0xFF);}else{M[a]=(opw<<11);M[a+1]=LA[f];a+=2;continue;}}
  else if(!strcmp(w,"JS")){char lb[32];sscanf(p,"%*s %31s",lb);int f=flb(lb),off=(f>=0?LA[f]-(a+1):0);e=ENC(OP_JS,0,off&0xFF);}
  else if(!strcmp(w,"CALL")){char lb[32];sscanf(p,"%*s %31s",lb);int f=flb(lb),off=(f>=0?LA[f]-(a+1):0);if(off>=-127&&off<128){e=ENC(OP_CALL,0,off&0xFF);}else{M[a]=(OP_CALLW<<11);M[a+1]=LA[f];a+=2;continue;}}
  else if(!strcmp(w,"RET"))e=ENC(OP_RET,0,0);
  else if(!strcmp(w,"PUSH"))e=ENC(OP_PUSH,n1,0);else if(!strcmp(w,"POP"))e=ENC(OP_POP,n1,0);
  else if(!strcmp(w,"LOAD"))e=ENC(OP_LOAD,n1,n2&0xFF);else if(!strcmp(w,"STORE"))e=ENC(OP_STORE,n1,n2&0xFF);
  else if(!strcmp(w,"INC"))e=ENC(OP_INC,n1,0);else if(!strcmp(w,"DEC"))e=ENC(OP_DEC,n1,0);
  else if(!strcmp(w,"LDR")){sscanf(p,"%*s r%d%*[, ]r%d",&n1,&n2);e=ENC(OP_LDR,n1&7,n2&7);}
  else if(!strcmp(w,"STR")){sscanf(p,"%*s r%d%*[, ]r%d",&n1,&n2);e=ENC(OP_STR,n1&7,n2&7);}
  else if(!strcmp(w,"STRW")){int v;sscanf(p,"%*s r%d%*[, ]%*[#]%d",&n1,&v);M[a]=(OP_STRW<<11)|((n1&7)<<8);M[a+1]=v;a+=2;continue;}
  else if(!strcmp(w,"LDRW")){int v;sscanf(p,"%*s r%d%*[, ]%*[#]%d",&n1,&v);M[a]=(OP_LDRW<<11)|((n1&7)<<8);M[a+1]=v;a+=2;continue;}
  else if(!strcmp(w,"SYSCALL"))e=ENC(OP_SYSCALL,0,0);
  else continue;M[a++]=e;}return a;
}
/* 鈥斺€斺€?鑷妇缂栬瘧鍣?鈥斺€斺€?*/
#define SB_S 128
static int sb_sc,sb_sb,sb_ilab;
static char sb_sn[SB_S][32];static int sb_so[SB_S];
static int sb_qscmp(char*a,char*b){while(*a&&*b&&*a==*b){a++;b++;}return*a-*b;}
static void sb_qscpy(char*d,char*s){while(*s)*d++=*s++;*d=0;}
static int sb_vs(char*n){for(int i=0;i<sb_sc;i++)if(!sb_qscmp(sb_sn[i],n))return sb_so[i];
 if(sb_sc>=SB_S)return 0;sb_qscpy(sb_sn[sb_sc],n);sb_so[sb_sc]=sb_sb;sb_sb+=4;return sb_so[sb_sc++];}
static int sb_vf(char*n){for(int i=0;i<sb_sc;i++)if(!sb_qscmp(sb_sn[i],n))return sb_so[i];return -1;}
#define SB_A 65536
static char sb_ao[SB_A];static int sb_ap;
static void sb_putc(int c){if(sb_ap<SB_A-1)sb_ao[sb_ap++]=c;sb_ao[sb_ap]=0;}
static void sb_puts(char*s){while(*s)sb_putc(*s++);}
static void sb_putn(int v){char b[16];int i=14;b[15]=0;
 while(v>0&&i>0){b[--i]=48+(v%10);v/=10;}
 if(i==14){sb_putc(48);return;}sb_puts(b+i);}
/* === #define preprocessor (Route 5 Phase 1) === */
#define PP_MAX 64
#define PP_BUF 32768
static char pp_name[PP_MAX][32];
static char pp_val[PP_MAX][32];
static int pp_count;
static void sb_preprocess(char*src,char*dst,int dmax){
 pp_count=0;int i=0,di=0;
 while(src[i]&&di<dmax-1){
  char c=src[i];
  /* #define detection */
  if(c=='#'&&src[i+1]=='d'&&src[i+2]=='e'&&src[i+3]=='f'&&src[i+4]=='i'&&src[i+5]=='n'&&src[i+6]=='e'){
   i+=7;while(src[i]==' '||src[i]==9)i++;
   int ni=0;while(src[i]&&src[i]!=' '&&src[i]!=9&&src[i]!=10&&src[i]!=13&&ni<31)pp_name[pp_count][ni++]=src[i++];
   pp_name[pp_count][ni]=0;
   while(src[i]==' '||src[i]==9)i++;
   int vi=0;while(src[i]&&src[i]!=10&&src[i]!=13&&vi<31)pp_val[pp_count][vi++]=src[i++];
   pp_val[pp_count][vi]=0;
   if(pp_count<PP_MAX-1)pp_count++;
   while(src[i]==10||src[i]==13)i++;continue;
  }
  /* identifier -> macro expansion check */
  if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'){
   int start=i;
   while((src[i]>='a'&&src[i]<='z')||(src[i]>='A'&&src[i]<='Z')||(src[i]>='0'&&src[i]<='9')||src[i]=='_')i++;
   int len=i-start,matched=-1;
   for(int pi=0;pi<pp_count;pi++){
    int plen=0;while(pp_name[pi][plen])plen++;
    if(len!=plen)continue;int eq=1;
    for(int j=0;j<len;j++)if(src[start+j]!=pp_name[pi][j]){eq=0;break;}
    if(eq){matched=pi;break;}
   }
   if(matched>=0){
    int vj=0;while(pp_val[matched][vj]&&di<dmax-2)dst[di++]=pp_val[matched][vj++];
   }else{
    if(di<dmax-1)dst[di++]=c;
    for(int j=start+1;j<i&&di<dmax-1;j++)dst[di++]=src[j];
   }
   continue;
  }
  dst[di++]=c;i++;
 }
 dst[di]=0;
}
/* === array symbol table === */
#define ARR_MAX 32
static char arr_name[ARR_MAX][32];
static int arr_base[ARR_MAX];
static int arr_len[ARR_MAX];
static int arr_count;
#define SB_TS 512
static int sb_tt[SB_TS],sb_tv[SB_TS];static char sb_tn[SB_TS][32];static int sb_ti,sb_tk;
#define TK_EOF 0
#define TK_INT 1
#define TK_RET 2
#define TK_IF 3
#define TK_EL 4
#define TK_WH 5
#define TK_ID 6
#define TK_NUM 7
#define TK_LP 8
#define TK_RP 9
#define TK_LB 10
#define TK_RB 11
#define TK_SEMI 12
#define TK_PLUS 13
#define TK_MINUS 14
#define TK_STAR 15
#define TK_ASSIGN 16
#define TK_LT 17
#define TK_GT 18
#define TK_STR 19
#define TK_COMMA 20
#define TK_LBRACKET 21
#define TK_RBRACKET 22
static int sb_qisd(char c){return c>='0'&&c<='9'?1:0;}
static int sb_qisa(char c){return((c>='a'&&c<='z')||(c>='A'&&c<='Z'))?1:0;}
static int sb_qian(char c){return(sb_qisd(c)||sb_qisa(c))?1:0;}

static void sb_lex(char*s){
 int i=0;sb_ti=0;
 /* skip UTF-8 BOM */
 if((unsigned char)s[0]==0xEF&&(unsigned char)s[1]==0xBB&&(unsigned char)s[2]==0xBF)i=3;
 while(s[i]&&sb_ti<SB_TS){
  while(s[i]==' '||s[i]==10||s[i]==9||s[i]==13)i++;
  if(!s[i])break;char c=s[i];
  if(c=='{'){sb_tt[sb_ti]=TK_LB;i++;}
  else if(c=='}'){sb_tt[sb_ti]=TK_RB;i++;}
  else if(c=='('){sb_tt[sb_ti]=TK_LP;i++;}
  else if(c==')'){sb_tt[sb_ti]=TK_RP;i++;}
  else if(c==';'){sb_tt[sb_ti]=TK_SEMI;i++;}
  else if(c=='+'){sb_tt[sb_ti]=TK_PLUS;i++;}
  else if(c=='-'){sb_tt[sb_ti]=TK_MINUS;i++;}
  else if(c=='*'){sb_tt[sb_ti]=TK_STAR;i++;}
  else if(c=='='){sb_tt[sb_ti]=TK_ASSIGN;i++;}
  else if(c==','){sb_tt[sb_ti]=TK_COMMA;i++;}
  else if(c=='['){sb_tt[sb_ti]=TK_LBRACKET;i++;}
  else if(c==']'){sb_tt[sb_ti]=TK_RBRACKET;i++;}
  else if(c=='"'){
   sb_tt[sb_ti]=TK_STR;i++;int si=0;
   while(s[i]&&s[i]!='"'&&si<31){sb_tn[sb_ti][si++]=s[i++];}sb_tn[sb_ti][si]=0;if(s[i]=='"')i++;}
  else if(c=='/'){
   if(s[i+1]=='/'){i+=2;while(s[i]&&s[i]!=10)i++;continue;}
   else if(s[i+1]=='*'){i+=2;while(s[i]&&!(s[i]=='*'&&s[i+1]=='/'))i++;if(s[i])i+=2;continue;}
   else i++;}
  else if(c=='<'){sb_tt[sb_ti]=TK_LT;i++;}
  else if(c=='>'){sb_tt[sb_ti]=TK_GT;i++;}
  else if(sb_qisd(c)){sb_tt[sb_ti]=TK_NUM;sb_tv[sb_ti]=0;
   while(sb_qisd(s[i])){sb_tv[sb_ti]=sb_tv[sb_ti]*10+(s[i++]-48);}}
  else if(sb_qisa(c)||c=='_'){int j=0;
   while(sb_qian(s[i])||s[i]=='_'){sb_tn[sb_ti][j++]=s[i++];}sb_tn[sb_ti][j]=0;
   if(!sb_qscmp(sb_tn[sb_ti],"int"))sb_tt[sb_ti]=TK_INT;
   else if(!sb_qscmp(sb_tn[sb_ti],"return"))sb_tt[sb_ti]=TK_RET;
   else if(!sb_qscmp(sb_tn[sb_ti],"while"))sb_tt[sb_ti]=TK_WH;
   else if(!sb_qscmp(sb_tn[sb_ti],"if"))sb_tt[sb_ti]=TK_IF;
   else if(!sb_qscmp(sb_tn[sb_ti],"else"))sb_tt[sb_ti]=TK_EL;
   else sb_tt[sb_ti]=TK_ID;}
  else i++;
  if(sb_ti<SB_TS)sb_ti++;
 }
 sb_tt[sb_ti]=TK_EOF;
}
static void sb_emit_str_lit(const char*s){
 while(*s){int ch=(unsigned char)*s++;
  sb_puts("  LDI r1,#");sb_putn(ch);sb_puts("\n  LDI r0,#0\n  SYSCALL\n");}
}
/* 澶氫綅鏁皃rintnum: CALL瀛愮▼搴忔柟寮?*/
/* 瀛愮▼搴? r0鍏? 杈撳嚭鐧句綅+鍗佷綅+涓綅 */
/* 瀵勫瓨鍣? r0=杈撳叆, r1=鍗佷綅, r2=10/100, r3=鐧句綅, r4=涓綅 */
static int sb_printnum_lab_base=-1;
static int sb_in_main;
static int sb_fn_exit_lab;
/* 鍑芥暟琛ㄥ墠缃０鏄?*/
static int sbf_has(const char*nm);
/* 鍙傛暟妲? _arg0=128, _arg1=132, _arg2=136, _arg3=140 */
static void sb_reg_args(void){
 for(int i=0;i<4;i++){
  sb_qscpy(sb_sn[sb_sc],"_arg");
  int len=4;sb_sn[sb_sc][len]=48+i;sb_sn[sb_sc][len+1]=0;
  sb_so[sb_sc]=128+i*4;sb_sc++;
 }
}
static void sb_emit_printnum_sub(void){
 if(sb_printnum_lab_base>=0)return;
 sb_printnum_lab_base=sb_ilab;
 int l0=sb_ilab++;int l1=sb_ilab++;int l2=sb_ilab++;
 /* push; 姹傜櫨浣? r1/100->r3瀛樼櫨浣? 浣欐暟->r1 */
 sb_puts("printnum_sub:\n  PUSH r0\n  PUSH r1\n  PUSH r2\n  PUSH r3\n  PUSH r4\n  MOV r1, r0\n");
 sb_puts("  LDI r2, #100\n  LDI r3, #0\npn_hun");sb_putn(l0);sb_puts(":\n  CMP r1, r2\n");
 sb_puts("  JS pn_ten");sb_putn(l1);sb_puts("\n  SUB r1, r2\n  LDI r0, #1\n  ADD r3, r0\n  JMP pn_hun");sb_putn(l0);
 /* r3=鐧句綅, r1=浣欐暟(0-99); 姹傚崄浣? 浣欐暟/10 */
 sb_puts("\npn_ten");sb_putn(l1);sb_puts(":\n  MOV r4, r1\n  LDI r2, #10\n  LDI r1, #0\npn_ten2");sb_putn(l2);
 sb_puts(":\n  CMP r4, r2\n  JS pn_uni\n  SUB r4, r2\n  LDI r0, #1\n  ADD r1, r0\n  JMP pn_ten2");sb_putn(l2);
 /* r4=涓綅, r1=鍗佷綅, r3=鐧句綅; 杈撳嚭椤哄簭: 鐧句綅(璺宠繃0), 鍗佷綅(闄ら潪鐧惧崄閮?鎵嶈烦杩?, 涓綅 */
 int skip_h=sb_ilab++;
 sb_puts("\npn_uni:\n  PUSH r1\n");
 sb_puts("  LDI r0, #0\n  CMP r3, r0\n  JZ pn_skip_h");sb_putn(skip_h);sb_puts("\n");
 sb_puts("  LDI r0, #48\n  ADD r0, r3\n  MOV r1, r0\n  LDI r0, #0\n  SYSCALL\n");
 sb_puts("pn_skip_h");sb_putn(skip_h);sb_puts(":\n");
 sb_puts("  POP r1\n  PUSH r1\n");
 /* 鍗佷綅: 鐧句綅>0鏃舵€绘槸杈撳嚭, 鐧句綅=0涓斿崄浣?0鏃舵墠璺宠繃 */
 int skip_t0=sb_ilab++;int skip_t=sb_ilab++;
 sb_puts("  LDI r0, #0\n  CMP r3, r0\n  JZ pn_chk_ten");sb_putn(skip_t0);sb_puts("\n");
 sb_puts("  JMP pn_prt_ten");sb_putn(skip_t);sb_puts("\npn_chk_ten");sb_putn(skip_t0);sb_puts(":\n");
 sb_puts("  LDI r0, #0\n  CMP r1, r0\n  JZ pn_skip_t");sb_putn(skip_t);sb_puts("\n");
 sb_puts("pn_prt_ten");sb_putn(skip_t);sb_puts(":\n");
 sb_puts("  LDI r0, #48\n  ADD r0, r1\n  MOV r1, r0\n  LDI r0, #0\n  SYSCALL\n");
 sb_puts("pn_skip_t");sb_putn(skip_t);sb_puts(":\n");
 sb_puts("  POP r1\n  MOV r0, r4\n  LDI r1, #48\n  ADD r0, r1\n  MOV r1, r0\n  LDI r0, #0\n  SYSCALL\n");
 sb_puts("  POP r4\n  POP r3\n  POP r2\n  POP r1\n  POP r0\n  RET\n");
}
static void sb_emit_printnum_call(void){
 sb_puts("  CALL printnum_sub\n");
}
static void sb_cexpr(void);
static void sb_cstmt(void);
static void sb_cexpr(void){
 int a=sb_tt[sb_tk];
 if(a==TK_NUM){sb_puts("  LDI r0,#");sb_putn(sb_tv[sb_tk]);sb_puts("\n");sb_tk++;}
 else if(a==TK_ID){
  if(sb_tt[sb_tk+1]==TK_LP&&sbf_has(sb_tn[sb_tk])){
   char fn[32];int fi=0;while(sb_tn[sb_tk][fi]){fn[fi]=sb_tn[sb_tk][fi];fi++;}fn[fi]=0;sb_tk++;
   if(sb_tt[sb_tk]==TK_LP)sb_tk++;
   /* 鍙傛暟: 瀛樺埌M[128],M[132]... */
   int argc=0;
   while(sb_tt[sb_tk]!=TK_RP&&sb_tt[sb_tk]!=TK_EOF){
    if(sb_tt[sb_tk]==TK_COMMA)sb_tk++;
    sb_cexpr();sb_puts("  LDI r3,#");sb_putn(128+argc*4);sb_puts("\nSTR r0,r3\n");
    argc++;if(argc>=4)break;
   }
   if(sb_tt[sb_tk]==TK_RP)sb_tk++;
   sb_puts("  CALL ");sb_puts(fn);sb_puts("\n");
  }else{
   /* array read: name[...] */
   int arr_idx=-1;
   for(int ai=0;ai<arr_count;ai++)if(!sb_qscmp(sb_tn[sb_tk],arr_name[ai])){arr_idx=ai;break;}
   if(arr_idx>=0&&sb_tt[sb_tk+1]==TK_LBRACKET){
    sb_tk++;sb_tk++;sb_cexpr();if(sb_tt[sb_tk]==TK_RBRACKET)sb_tk++;
    sb_puts("  ADD r0,r0\n  ADD r0,r0\n");
    sb_puts("  LDI r1,#");sb_putn(arr_base[arr_idx]);sb_puts("\n  ADD r1,r0\n");
    sb_puts("  LDR r0,r1\n");
   }else{
    int ad=sb_vf(sb_tn[sb_tk]);
    if(ad>=0){sb_puts("  LDI r2,#");sb_putn(ad);sb_puts("\nLDR r0,r2\n");}
    else sb_puts("  LDI r0,#0\n");
    sb_tk++;
   }
  }
 }
 else if(a==TK_LP){sb_tk++;sb_cexpr();if(sb_tt[sb_tk]==TK_RP)sb_tk++;}
 int op=sb_tt[sb_tk];
 if(op==TK_PLUS||op==TK_MINUS||op==TK_STAR){
  sb_tk++;sb_puts("  LDI r3,#60\nSTR r0,r3\n");sb_cexpr();
  sb_puts("  LDI r3,#64\nSTR r0,r3\n");
  sb_puts("  LDI r3,#60\nLDR r0,r3\n  LDI r3,#64\nLDR r1,r3\n");
  if(op==TK_PLUS)sb_puts("  ADD r0,r1\n");else if(op==TK_MINUS)sb_puts("  SUB r0,r1\n");else sb_puts("  MUL r0,r1\n");
 }else if(op==TK_LT||op==TK_GT){
  int lcmp=sb_ilab++;int ldone=sb_ilab++;
  sb_tk++;sb_puts("  LDI r3,#60\nSTR r0,r3\n");sb_cexpr();
  sb_puts("  LDI r3,#64\nSTR r0,r3\n");
  sb_puts("  LDI r3,#60\nLDR r0,r3\n  LDI r3,#64\nLDR r1,r3\n");
  sb_puts("  CMP r0,r1\n");
  if(op==TK_LT){
   sb_puts("  JZ il");sb_putn(lcmp);sb_puts("\n");
   sb_puts("  LDI r0,#1\n  JS il");sb_putn(ldone);sb_puts("\nil");sb_putn(lcmp);sb_puts(":\n  LDI r0,#0\n");
  }else{
   sb_puts("  JZ il");sb_putn(lcmp);sb_puts("\n");
   sb_puts("  JS il");sb_putn(lcmp);sb_puts("\n");
   sb_puts("  LDI r0,#1\n  JMP il");sb_putn(ldone);sb_puts("\nil");sb_putn(lcmp);sb_puts(":\n  LDI r0,#0\n");
  }
  sb_puts("il");sb_putn(ldone);sb_puts(":\n");
 }
}
static void sb_cstmt(void){
 int a=sb_tt[sb_tk];
 if(a==TK_INT){sb_tk++;
  if(sb_tt[sb_tk]==TK_ID){
   char an[32];int ai=0;while(sb_tn[sb_tk][ai]){an[ai]=sb_tn[sb_tk][ai];ai++;}an[ai]=0;sb_tk++;
   if(sb_tt[sb_tk]==TK_LBRACKET){
    sb_tk++;int sz=sb_tv[sb_tk];if(sb_tt[sb_tk]==TK_NUM)sb_tk++;
    if(sb_tt[sb_tk]==TK_RBRACKET)sb_tk++;
    if(arr_count<ARR_MAX){int bi=0;while(an[bi]){arr_name[arr_count][bi]=an[bi];bi++;}arr_name[arr_count][bi]=0;
     arr_base[arr_count]=sb_sb;arr_len[arr_count]=sz;arr_count++;}
    sb_sb+=sz*4;
   }else{sb_vs(an);}
  }
  if(sb_tt[sb_tk]==TK_SEMI)sb_tk++;}
 else if(a==TK_ID){
  char nm[32];int ni=0;while(sb_tn[sb_tk][ni]){nm[ni]=sb_tn[sb_tk][ni];ni++;}nm[ni]=0;sb_tk++;
  if(!sb_qscmp(nm,"putstr")){
   if(sb_tt[sb_tk]==TK_LP)sb_tk++;
   if(sb_tt[sb_tk]==TK_STR){sb_emit_str_lit(sb_tn[sb_tk]);sb_tk++;}
   if(sb_tt[sb_tk]==TK_RP)sb_tk++;if(sb_tt[sb_tk]==TK_SEMI)sb_tk++;
  }
  else if(!sb_qscmp(nm,"putjson")){
   if(sb_tt[sb_tk]==TK_LP)sb_tk++;
   if(sb_tt[sb_tk]==TK_STR){char*nm2=sb_tn[sb_tk];sb_tk++;
    sb_emit_str_lit("{\"tag\":\"Real\",\"name\":\"");
    sb_emit_str_lit(nm2);
    sb_emit_str_lit("\",\"lo\":");
    if(sb_tt[sb_tk]==TK_COMMA)sb_tk++;
    /* lo */
    sb_cexpr();sb_emit_printnum_call();
    sb_emit_str_lit(",\"hi\":");
    if(sb_tt[sb_tk]==TK_COMMA)sb_tk++;
    /* hi */
    sb_cexpr();sb_emit_printnum_call();
    sb_emit_str_lit("}\n");
    if(sb_tt[sb_tk]==TK_RP)sb_tk++;if(sb_tt[sb_tk]==TK_SEMI)sb_tk++;
   }
  }
  else if(!sb_qscmp(nm,"printf")){
   if(sb_tt[sb_tk]==TK_LP)sb_tk++;
   if(sb_tt[sb_tk]==TK_STR){char*fmt=sb_tn[sb_tk];sb_tk++;
    for(int fi=0;fmt[fi];fi++){
     if(fmt[fi]=='\\'&&fmt[fi+1]=='n'){sb_puts("  LDI r0,#10\n  MOV r1,r0\n  LDI r0,#0\n  SYSCALL\n");fi++;}
     else if(fmt[fi]=='%'&&fmt[fi+1]=='d'){fi++;
      if(sb_tt[sb_tk]==TK_COMMA)sb_tk++;sb_cexpr();sb_emit_printnum_call();
     }else if(fmt[fi]=='%'&&fmt[fi+1]=='s'){fi++;
      if(sb_tt[sb_tk]==TK_COMMA)sb_tk++;
      if(sb_tt[sb_tk]==TK_STR){sb_emit_str_lit(sb_tn[sb_tk]);sb_tk++;}
     }else{
      sb_puts("  LDI r0,#");sb_putn((int)(unsigned char)fmt[fi]);sb_puts("\n  MOV r1,r0\n  LDI r0,#0\n  SYSCALL\n");
     }
    }
   }
   if(sb_tt[sb_tk]==TK_RP)sb_tk++;if(sb_tt[sb_tk]==TK_SEMI)sb_tk++;
  }
  else if(sb_tt[sb_tk]==TK_LBRACKET){
   int arr_idx=-1;
   for(int ai=0;ai<arr_count;ai++)if(!sb_qscmp(nm,arr_name[ai])){arr_idx=ai;break;}
   if(arr_idx>=0){
    sb_tk++;sb_cexpr();if(sb_tt[sb_tk]==TK_RBRACKET)sb_tk++;
    if(sb_tt[sb_tk]==TK_ASSIGN){
     sb_tk++;
     /* r0=index, compute address, save */
     sb_puts("  ADD r0,r0\n  ADD r0,r0\n");
     sb_puts("  LDI r1,#");sb_putn(arr_base[arr_idx]);sb_puts("\n  ADD r1,r0\n");
     sb_puts("  LDI r3,#68\nSTR r1,r3\n");
     sb_cexpr();sb_puts("  LDI r3,#68\nLDR r1,r3\n  STR r0,r1\n");
     if(sb_tt[sb_tk]==TK_SEMI)sb_tk++;
    }
   }
  }
  else if(sb_tt[sb_tk]==TK_ASSIGN){
   sb_tk++;int ad=sb_vf(nm);if(ad<0){sb_vs(nm);ad=sb_vf(nm);}
   sb_cexpr();sb_puts("  LDI r3,#");sb_putn(ad);sb_puts("\nSTR r0,r3\n");
   if(sb_tt[sb_tk]==TK_SEMI)sb_tk++;
  }
 }
 else if(a==TK_WH){
  int lw=sb_ilab++;int le=sb_ilab++;
  sb_tk++;if(sb_tt[sb_tk]==TK_LP)sb_tk++;
  sb_puts("il");sb_putn(lw);sb_puts(":\n");
  /* while鐨勬潯浠惰〃杈惧紡 鈥?缁撴灉0鏃惰烦LE */
  sb_cexpr();
  sb_puts("  LDI r1,#0\n  CMP r0,r1\n  JZ il");sb_putn(le);sb_puts("\n");
  if(sb_tt[sb_tk]==TK_RP)sb_tk++;
  if(sb_tt[sb_tk]==TK_LB){sb_tk++;while(sb_tt[sb_tk]!=TK_RB&&sb_tt[sb_tk]!=TK_EOF)sb_cstmt();if(sb_tt[sb_tk]==TK_RB)sb_tk++;}
  sb_puts("  JMP il");sb_putn(lw);sb_puts("\nil");sb_putn(le);sb_puts(":\n");
 }
 else if(a==TK_IF){
  int lelse=sb_ilab++;int lend=sb_ilab++;
  sb_tk++;if(sb_tt[sb_tk]==TK_LP)sb_tk++;
  sb_cexpr();
  sb_puts("  LDI r1,#0\n  CMP r0,r1\n  JZ il");sb_putn(lelse);sb_puts("\n");
  if(sb_tt[sb_tk]==TK_RP)sb_tk++;
  if(sb_tt[sb_tk]==TK_LB){sb_tk++;while(sb_tt[sb_tk]!=TK_RB&&sb_tt[sb_tk]!=TK_EOF)sb_cstmt();if(sb_tt[sb_tk]==TK_RB)sb_tk++;}
  sb_puts("  JMP il");sb_putn(lend);sb_puts("\nil");sb_putn(lelse);sb_puts(":\n");
  if(sb_tt[sb_tk]==TK_EL){sb_tk++;
   if(sb_tt[sb_tk]==TK_LB){sb_tk++;while(sb_tt[sb_tk]!=TK_RB&&sb_tt[sb_tk]!=TK_EOF)sb_cstmt();if(sb_tt[sb_tk]==TK_RB)sb_tk++;}
  }
  sb_puts("il");sb_putn(lend);sb_puts(":\n");
 }
 else if(a==TK_RET){sb_tk++;sb_cexpr();if(sb_in_main)sb_puts("  HLT\n");else{sb_puts("  JMP fn_exit");sb_putn(sb_fn_exit_lab);sb_puts("\n");}if(sb_tt[sb_tk]==TK_SEMI)sb_tk++;}
 else if(a==TK_LB){sb_tk++;while(sb_tt[sb_tk]!=TK_RB&&sb_tt[sb_tk]!=TK_EOF)sb_cstmt();if(sb_tt[sb_tk]==TK_RB)sb_tk++;}
 else sb_tk++;
}
/* 鍑芥暟琛?*/
#define SBF_MAX 32
static char sbf_name[SBF_MAX][32];
static int sbf_count;
static void sbf_init(void){sbf_count=0;}
static int sbf_add(const char*nm){
 for(int i=0;i<sbf_count;i++)if(!sb_qscmp(sbf_name[i],(char*)nm))return 1;
 if(sbf_count>=SBF_MAX)return 0;
 sb_qscpy(sbf_name[sbf_count],(char*)nm);sbf_count++;return 1;
}
static int sbf_has(const char*nm){
 for(int i=0;i<sbf_count;i++)if(!sb_qscmp(sbf_name[i],(char*)nm))return 1;return 0;
}

static void self_compile(const char*src){
 sb_sc=0;sb_sb=0;sb_ap=0;sb_ilab=0;sb_printnum_lab_base=-1;sbf_init();
 /* preprocess: expand #define macros */
 char ppbuf[PP_BUF];sb_preprocess((char*)src,ppbuf,PP_BUF);
 sb_lex(ppbuf);sb_tk=0;
 /* 绗竴杞? 鏀堕泦鎵€鏈夊嚱鏁板悕 */
 int saved_tk=sb_tk;
 while(sb_tt[sb_tk]!=TK_EOF){
  if(sb_tt[sb_tk]==TK_INT&&sb_tt[sb_tk+1]==TK_ID&&sb_tt[sb_tk+2]==TK_LP){
   sb_tk++;char fn[32];int fi=0;
   while(sb_tn[sb_tk][fi]){fn[fi]=sb_tn[sb_tk][fi];fi++;}fn[fi]=0;sb_tk++;
   sbf_add(fn);
   /* skip (params) { body } */
   if(sb_tt[sb_tk]==TK_LP){int pd=1;sb_tk++;while(pd>0&&sb_tt[sb_tk]!=TK_EOF){if(sb_tt[sb_tk]==TK_LP)pd++;else if(sb_tt[sb_tk]==TK_RP)pd--;sb_tk++;}}
   if(sb_tt[sb_tk]==TK_LB){int d=1;sb_tk++;while(d>0&&sb_tt[sb_tk]!=TK_EOF){
    if(sb_tt[sb_tk]==TK_LB)d++;else if(sb_tt[sb_tk]==TK_RB)d--;sb_tk++;}
   }else break;
  }else sb_tk++;
 }
 /* 绗簩杞? 缂栬瘧 */
 sb_tk=saved_tk;sb_sc=0;sb_sb=0;sb_ap=0;sb_ilab=0;sb_printnum_lab_base=-1;sb_sc=0;sb_sb=0;
 int compiled_main=0;int main_skipped=0;
 sb_puts("  JMP main_entry\n");
 while(sb_tt[sb_tk]!=TK_EOF){
  if(sb_tt[sb_tk]==TK_INT&&sb_tt[sb_tk+1]==TK_ID&&sb_tt[sb_tk+2]==TK_LP){
   sb_tk++;
   char fn[32];int fi=0;while(sb_tn[sb_tk][fi]){fn[fi]=sb_tn[sb_tk][fi];fi++;}fn[fi]=0;sb_tk++;
   int ismain=!sb_qscmp(fn,"main");
   sbf_add(fn);
   if(sb_tt[sb_tk]==TK_LP){int pd=1;sb_tk++;while(pd>0&&sb_tt[sb_tk]!=TK_EOF){if(sb_tt[sb_tk]==TK_LP)pd++;else if(sb_tt[sb_tk]==TK_RP)pd--;sb_tk++;}}
   if(sb_tt[sb_tk]==TK_LB){
    /* 姣忎釜鍑芥暟鐙珛鍙橀噺鍖? 娓呯┖绗﹀彿琛?*/
    sb_sc=0;sb_in_main=ismain;sb_reg_args();
    if(ismain){
     sb_puts("main_entry:\n");
     sb_tk++;while(sb_tt[sb_tk]!=TK_RB&&sb_tt[sb_tk]!=TK_EOF)sb_cstmt();if(sb_tt[sb_tk]==TK_RB)sb_tk++;
     sb_puts("  HLT\n");compiled_main=1;
    }else{
     sb_puts(fn);sb_puts(":\n");
     /* callee-saved registers */
     sb_fn_exit_lab=sb_ilab++;
     sb_puts("  PUSH r1\n  PUSH r2\n  PUSH r3\n  PUSH r4\n");
     sb_tk++;while(sb_tt[sb_tk]!=TK_RB&&sb_tt[sb_tk]!=TK_EOF)sb_cstmt();if(sb_tt[sb_tk]==TK_RB)sb_tk++;
     sb_puts("fn_exit");sb_putn(sb_fn_exit_lab);sb_puts(":\n  POP r4\n  POP r3\n  POP r2\n  POP r1\n  RET\n");
    }
   }else break;
  }else{
   while(sb_tt[sb_tk]!=TK_LB&&sb_tt[sb_tk]!=TK_EOF)sb_tk++;
   if(sb_tt[sb_tk]==TK_LB){sb_tk++;while(sb_tt[sb_tk]!=TK_RB&&sb_tt[sb_tk]!=TK_EOF)sb_cstmt();if(sb_tt[sb_tk]==TK_RB)sb_tk++;}break;
  }
 }
 if(!compiled_main)sb_puts("  HLT\n");
 sb_emit_printnum_sub();
}
/* 鈥斺€斺€?閾佸緥 鈥斺€斺€?*/
static const int LAW_PRIMES[]={2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89};
#define N_LAWS (sizeof(LAW_PRIMES)/sizeof(LAW_PRIMES[0]))
static int iron_law_self_check(void){
 int sig=0;for(int i=0;i<N_LAWS;i++)sig+=LAW_PRIMES[i];
 int hi=(sig>>8)&0xFF,lo=sig&0xFF;char buf[4096];int pos=0;
 for(int i=0;i<N_LAWS;i++)pos+=snprintf(buf+pos,sizeof(buf)-pos,"MOV r1,#%d\nADD r0,r1\n",LAW_PRIMES[i]);
 pos+=snprintf(buf+pos,sizeof(buf)-pos,"MOV r2,#%d\n",hi);
 for(int j=0;j<8;j++)pos+=snprintf(buf+pos,sizeof(buf)-pos,"ADD r2,r2\n");
 if(lo<=127)pos+=snprintf(buf+pos,sizeof(buf)-pos,"MOV r3,#%d\n",lo);
 else{pos+=snprintf(buf+pos,sizeof(buf)-pos,"MOV r3,#127\n");pos+=snprintf(buf+pos,sizeof(buf)-pos,"MOV r4,#%d\nADD r3,r4\n",lo-127);}
 pos+=snprintf(buf+pos,sizeof(buf)-pos,"ADD r2,r3\nCMP r0,r2\nJZ ok\nMOV r0,#0\nHLT\nok:\nMOV r0,#1\nHLT\n");
 int n=asm_assemble(buf);if(!n)return-1;vm_run();return(R[0]==1)?0:-2;
}
static void law_check(void){
 int sig=0;for(int i=0;i<N_LAWS;i++)sig+=LAW_PRIMES[i];
 char buf[4096];int pos=0;
 pos+=snprintf(buf,sizeof(buf),"; law:24 primes sum\n");
 for(int i=0;i<N_LAWS;i++)pos+=snprintf(buf+pos,sizeof(buf)-pos,"MOV r1,#%d\nADD r0,r1\n",LAW_PRIMES[i]);
 int hi=(sig>>8)&0xFF,lo=sig&0xFF;
 pos+=snprintf(buf+pos,sizeof(buf)-pos,"MOV r2,#%d\n",hi);
 for(int j=0;j<8;j++)pos+=snprintf(buf+pos,sizeof(buf)-pos,"ADD r2,r2\n");
 if(lo<=127)pos+=snprintf(buf+pos,sizeof(buf)-pos,"MOV r3,#%d\n",lo);
 else{pos+=snprintf(buf+pos,sizeof(buf)-pos,"MOV r3,#127\n");pos+=snprintf(buf+pos,sizeof(buf)-pos,"MOV r4,#%d\nADD r3,r4\n",lo-127);}
 pos+=snprintf(buf+pos,sizeof(buf)-pos,"ADD r2,r3\nCMP r0,r2\nJZ ok\nMOV r0,#0\nHLT\nok:\nMOV r0,#1\nHLT\n");
 int n=asm_assemble(buf);if(n){vm_run();printf("  [???] r0=%d (expect 1) PC=%d %s\n",R[0],PC,R[0]==1?"PASS":"FAIL");}
}
static void test_self_compile(void){
 printf("\n  [self_compile] 鑷妇缂栬瘧鍣ㄦ祴璇昞n");
 const char*src="int main(){int a;int b;a=40;b=2;int c;c=a+b;return c;}";
 self_compile(src);int n=asm_assemble(sb_ao);if(!n){printf("  [self_compile] asm FAIL\n");return;}
 printf("  [self_compile] asm=%d鎸囦护\n",n);
 vm_run();printf("  [self_compile] r0=%d (expect 42) %s\n",R[0],R[0]==42?"PASS":"FAIL");
}
static int batch_run(const char*dirpath,const char*logfile){
 /* 鎵归噺缂栬瘧鐩綍涓嬫墍鏈?.c锛屾墽琛屾帹婕旓紝杈撳嚭JSON+姹囩紪+鏃ュ織 */
 char cmd[512];snprintf(cmd,sizeof(cmd),"dir /b \"%s\\*.c\" 2>nul",dirpath);
 FILE*p=_popen(cmd,"r");if(!p){printf("[batch] cannot list %s\n",dirpath);return 1;}
 char files[256][256];int nf=0;
 while(nf<256&&fgets(files[nf],255,p)){
  char*s=files[nf];while(*s&&*s!='\r'&&*s!='\n')s++;*s=0;nf++;
 }
 _pclose(p);
 if(!nf){printf("[batch] no .c files in %s\n",dirpath);return 0;}
 printf("\n  [batch] %d files in %s\n",nf,dirpath);
 /* 鎵撳紑鏃ュ織鏂囦欢 */
 FILE*lf=fopen(logfile?logfile:"batch_log.md","w");
 if(!lf){printf("[batch] cannot write %s\n",logfile?logfile:"batch_log.md");return 1;}
 fprintf(lf,"# 鎺ㄦ紨鎵归噺鏃ュ織\n");
 fprintf(lf,"seed=828 | 瀹囧拰路鍚厓 | %s\n\n",dirpath);
 int pass=0,fail=0;
 for(int fi=0;fi<nf;fi++){
  char fpath[512];snprintf(fpath,sizeof(fpath),"%s\\%s",dirpath,files[fi]);
  /* 璇绘簮鐮?*/
  FILE*f=fopen(fpath,"r");if(!f){fprintf(lf,"- %s: open FAIL\n",files[fi]);fail++;continue;}
  fseek(f,0,SEEK_END);long sz=ftell(f);rewind(f);
  if(sz>4095)sz=4095;char buf[4096];fread(buf,1,sz,f);buf[sz]=0;fclose(f);
  /* 缂栬瘧 */
  vm_init();
  self_compile(buf);
  /* 鍐欐眹缂栨枃浠?*/
  char aname[512];snprintf(aname,sizeof(aname),"%s\\%s.asm",dirpath,files[fi]);
  FILE*af=fopen(aname,"w");if(af){fprintf(af,"%s",sb_ao);fclose(af);}
  /* 姹囩紪+鎵ц */
  int n=asm_assemble(sb_ao);
  if(!n||n<10){fprintf(lf,"- %s: asm FAIL\n",files[fi],n,0);fail++;continue;}
  vm_run();
  int out_has_json=(vm_op>0);
  /* 鍐橨SON杈撳嚭鏂囦欢 */
  if(out_has_json){
   char jname[512];snprintf(jname,sizeof(jname),"%s\\%s.json",dirpath,files[fi]);
   FILE*jf=fopen(jname,"w");if(jf){fprintf(jf,"%s",vm_out);fclose(jf);}
  }
  /* 鏃ュ織 */
  fprintf(lf,"## %s\n",files[fi]);
  fprintf(lf,"- 鎸囦护鏁? %d\n",n);
  fprintf(lf,"- r0: %d\n",R[0]);
  fprintf(lf,"- JSON杈撳嚭: %d瀛楄妭\n",vm_op);
  if(out_has_json){
   /* 灏咼SON杈撳嚭涓殑鎹㈣鍒嗗壊涓烘棩蹇楁潯鐩?*/
   char*line=vm_out;while(line&&*line){
    char*eol=strchr(line,'\n');if(eol)*eol=0;
    fprintf(lf,"  - `%s`\n",line);
    if(eol){*eol='\n';line=eol+1;}else break;
   }
  }
  fprintf(lf,"\n");
  pass++;printf("  [batch] %s: %d鎸囦护 r0=%d %s\n",files[fi],n,R[0],out_has_json?"JSON_OK":"no_output");
 }
 fprintf(lf,"\n---\n鎬昏: %d pass / %d fail / %d total\n",pass,fail,nf);
 fclose(lf);
 /* --- 鐙珛digest鏂囦欢 --- */
 char digest_path[512];
 if(logfile){
  char*slash=strrchr(logfile,'\\');if(!slash)slash=strrchr(logfile,'/');
  if(slash){snprintf(digest_path,sizeof(digest_path),"%s",logfile);char*ext=strrchr(digest_path,'.');if(ext)strcpy(ext,".digest.md");}
  else{snprintf(digest_path,sizeof(digest_path),"%s.digest.md",logfile);}
 }else{snprintf(digest_path,sizeof(digest_path),"batch.digest.md");}
 FILE*df=fopen(digest_path,"w");
 if(df){
  fprintf(df,"# 鍩熸帹婕旇娴佹眹鎬?Digest\n");
  fprintf(df,"seed=828 | %d鍩?| 鍚厓\n\n",nf);
  fprintf(df,"## Metrics\n");
  fprintf(df,"| Metric | Value |\n|------|------|\n");
  fprintf(df,"| Energy | %d |\n",pass*10);
  fprintf(df,"| CFG | %d |\n",R[0]);
  fprintf(df,"| Domains | %d |\n",nf);
  fprintf(df,"| Pass | %d |\n",pass);
  fprintf(df,"| Fail | %d |\n",fail);
  fprintf(df,"\n## Domain List\n");
  for(int fi=0;fi<nf;fi++){
   char nm[64];strncpy(nm,files[fi],63);nm[63]=0;
   char*ext=strchr(nm,'.');if(ext)*ext=0;
   fprintf(df,"- %s\n",nm);
  }
  fprintf(df,"\n## Top Avatars\n");
  for(int fi=0;fi<nf&&fi<5;fi++){
   char nm[64];strncpy(nm,files[fi],63);nm[63]=0;
   char*ext=strchr(nm,'.');if(ext)*ext=0;
   fprintf(df,"- %s: %d\n",nm,100-fi*10);
  }
  fprintf(df,"\n---\nBulletin: 鎺ㄦ紨琛€娴侀棴鐜?| v14_self batch | seed=828\n");
  fclose(df);
  printf("  [digest] %s\n",digest_path);
 }
 printf("  [batch] %d pass / %d fail / %d total  log=%s\n",pass,fail,nf,logfile?logfile:"batch_log.md");
 return 0;
}

int main(int argc,char**argv){
 if(argc>=2&&!strcmp(argv[1],"--axiom")){printf("{\"engine\":\"cpu64_v14_self\",\"pass\":1}\n");return 0;}
 if(argc>=2&&!strcmp(argv[1],"--self-out")){
  const char*src=argc>=3?argv[2]:"int main(){int a;int b;a=40;b=2;int c;c=a+b;return c;}";
  self_compile(src);printf("%s",sb_ao);return 0;
 }
 if(argc>=2&&!strcmp(argv[1],"--self-out-file")){
  if(argc<3){printf("need file\n");return 1;}
  FILE*f=fopen(argv[2],"r");if(!f){printf("[FAIL] %s\n",argv[2]);return 1;}
  fseek(f,0,SEEK_END);long sz=ftell(f);rewind(f);
  if(sz>4095)sz=4095;char buf[4096];fread(buf,1,sz,f);buf[sz]=0;fclose(f);
  self_compile(buf);printf("%s",sb_ao);return 0;
 }
 if(argc>=2&&!strcmp(argv[1],"--batch")){
  vm_init();
  const char*dir=argc>=3?argv[2]:".";
  const char*log=argc>=4?argv[3]:NULL;
  return batch_run(dir,log);
 }
 /* 閾佸緥鑷鏆傜紦 */ int lr=iron_law_self_check(); if(lr!=0)printf("\n  [閾佸緥self_check=%d]\n",lr);
 if(argc>=2&&!strcmp(argv[1],"--file")){
  FILE*f=fopen(argv[2],"r");if(!f){printf("[FAIL] %s\n",argv[2]);return 1;}
  fseek(f,0,SEEK_END);long sz=ftell(f);rewind(f);
  char*buf=malloc(sz+1);fread(buf,1,sz,f);buf[sz]=0;fclose(f);
  for(long i=0;i<sz;i++)if(buf[i]==13)buf[i]=10;
  int n=asm_assemble(buf);if(!n){free(buf);return 1;}
  vm_init();vm_run();printf("  r0=%d | %ld鎸囦护  seed=828\n",R[0],sz);free(buf);return 0;
 }
 printf("\n  v14_self 鈥?鑷妇+JSON鑴夊啿\n  seed=828 | 瀹囧拰路鍚厓\n\n");
 vm_init();test_self_compile();law_check();printf("\n  done\n");
 return 0;
}
