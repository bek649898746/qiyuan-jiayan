// v57 — Agent 调度器 (sched 模块, 0.2 剩余)
// Token 预算轮转 + KV 感知切换. 3 Agent: think/code/review.
#include "srclib/kernel/serial.h"
#include "srclib/kernel/mem.h"

#define TOKEN_THINK   128
#define TOKEN_CODE    4096
#define TOKEN_REVIEW  1024

/* Agent 状态 (RAM 固定表, 每项 24 字节) */
#define AGT_BASE 0x3050000
#define AGT_NUM 3
/* 布局: name[8]@0, budget@8, used@12, kv_slot@16, state@20 (0=run 1=wait) */

空 sched_init(void){
    整 i=0;
    循环(i<AGT_NUM*24){*(字节*)(AGT_BASE+i)=0;i=i+1;}
    /* think: 128 token */
    *(整*)(AGT_BASE+8)=TOKEN_THINK;
    /* code: 4096 */
    *(整*)(AGT_BASE+1*24+8)=TOKEN_CODE;
    /* review: 1024 */
    *(整*)(AGT_BASE+2*24+8)=TOKEN_REVIEW;
    /* 名字 */
    *(字节*)(AGT_BASE+0)=1;   /* 用编号代替名字 (省串) */
    *(字节*)(AGT_BASE+24+0)=2;
    *(字节*)(AGT_BASE+48+0)=3;
    *(整*)(AGT_BASE+16)=0;    /* think KV0 */
    *(整*)(AGT_BASE+40)=1;    /* code KV1 */
    *(整*)(AGT_BASE+64)=2;    /* review KV2 */
}

/* 运行一个 Agent 一步: 消耗 budget, 返 0=预算耗尽需切换 */
整 sched_step(整 agt){
    整 e=AGT_BASE+agt*24;
    整 budget=*(整*)(e+8);
    整 used=*(整*)(e+12);
    used=used+1;
    *(整*)(e+12)=used;
    返 used<budget;  /* 0=耗尽 */
}

/* 调度一轮: 按剩余预算比例轮转 (简单版: 依次跑, 耗尽切下一个) */
空 sched_round(整 c, 整 *order, 整 order_n, 整 *result){
    整 i=0;
    循环(i<order_n){
        整 agt=order[i];
        整 r=sched_step(agt);
        result[i]=r;   /* 0=耗尽 1=还有 */
        i=i+1;
    }
}

空 _start(空){
    整 c=0x3F8;
    serial_init(c);
    serial_puts(c,"v57\n");
    mem_init();
    sched_init();
    /* 第一轮: think(128)/code(4096)/review(1024) — 每步消耗 1, 都还有 */
    整 order[3]; order[0]=0; order[1]=1; order[2]=2;
    整 res[3];
    sched_round(c,order,3,res);
    serial_puts(c,"[r1]");serial_num(c,res[0]);serial_num(c,res[1]);serial_num(c,res[2]);serial_putc(c,10);
    /* think 预算 128 → 跑 127 步后第 128 步耗尽 */
    整 i=1;
    循环(i<128){
        sched_step(0);
        i=i+1;
    }
    整 r=sched_step(0);
    serial_puts(c,"[t128]");serial_num(c,r);serial_putc(c,10);
    /* review 跑 1024 步 */
    整 j=0;
    循环(j<1024){
        sched_step(2);
        j=j+1;
    }
    整 r2=sched_step(2);
    serial_puts(c,"[rv]");serial_num(c,r2);serial_putc(c,10);
    /* 判定 */
    整 ok=1;
    若(res[0]!=1||res[1]!=1||res[2]!=1){ok=0;}   /* 第一轮都有预算 */
    若(r!=0){ok=0;}                               /* think 第 129 步耗尽 */
    若(r2!=0){ok=0;}                              /* review 第 1025 步耗尽 */
    若(*(整*)(AGT_BASE+12)!=129){ok=0;}           /* think used=129 */
    若(ok==1){serial_puts(c,"SCHED-PASS\n");}否则{serial_puts(c,"SCHED-FAIL\n");}
    循环(1){__asm(0xF4);}
}
