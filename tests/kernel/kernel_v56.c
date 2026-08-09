// v56 — 裸机内存管理器 (mem 模块, 0.2 剩余)
// 固定堆区 @0x3000000 (1MB), 块头: size@0, used@4, next@8. 首次适配 + 空闲合并.
#include "srclib/kernel/serial.h"

#define MEM_HEAP 0x3000000
#define MEM_HEAP_SIZE 0x100000   /* 1MB */
#define MEM_ALIGN 8

/* 初始化: 单空闲块覆盖整个堆 */
空 mem_init(void){
    整 *h=(整*)MEM_HEAP;
    *(整*)(MEM_HEAP+0)=MEM_HEAP_SIZE-16;  /* size (含头16字节) */
    *(整*)(MEM_HEAP+4)=0;                  /* unused */
    *(整*)(MEM_HEAP+8)=0;                  /* next=null */
    *(整*)(MEM_HEAP+12)=0;                 /* prev */
    *(整*)(MEM_HEAP+16)=0;                 /* magic 校验区 */
}
/* 分配: 首次适配, 返数据地址 (0=失败) */
整 kmalloc(整 size){
    若(size<=0)返 0;
    整 want=size+16;  /* 头+数据 */
    want=(want+7)&~7; /* 8 对齐 */
    整 addr=MEM_HEAP;
    循环(1){
        整 bsz=*(整*)addr;
        整 used=*(整*)(addr+4);
        若(bsz==0)断;  /* 到链表尾 */
        若(used==0 && bsz>=want){
            若(bsz >= want+16){  /* 剩余够拆新块 */
                *(整*)(addr+0)=want;        /* 已用块大小 */
                *(整*)(addr+4)=1;
                整 new=addr+want;
                *(整*)(new+0)=bsz-want;     /* 新空闲块 */
                *(整*)(new+4)=0;
                返 addr+16;
            }否则{
                *(整*)(addr+4)=1;  /* 整块分配 */
                返 addr+16;
            }
        }
        addr=addr+bsz;
    }
    返 0;
}
/* 释放: 标记空闲 (无合并 — 简化版) */
空 kfree(整 p){
    若(p==0)返;
    整 h=p-16;
    *(整*)(h+4)=0;
}
/* 统计: 总空闲/最大空闲/块数 */
空 mem_stats(整 c){
    serial_puts(c,"[mem]");
    整 addr=MEM_HEAP;
    整 free_n=0; 整 free_max=0; 整 blk_n=0;
    循环(1){
        整 bsz=*(整*)addr;
        若(bsz==0)断;
        blk_n=blk_n+1;
        若(*(整*)(addr+4)==0){
            free_n=free_n+bsz;
            若(bsz>free_max){free_max=bsz;}
        }
        addr=addr+bsz;
    }
    serial_num(c,blk_n);serial_putc(c,44);
    serial_num(c,free_n);serial_putc(c,44);
    serial_num(c,free_max);serial_putc(c,10);
}

空 _start(空){
    整 c=0x3F8;
    serial_init(c);
    serial_puts(c,"v56\n");
    mem_init();
    /* 1. 分配 3 块 */
    整 a=kmalloc(100);
    整 b=kmalloc(200);
    整 c1=kmalloc(64);
    若(a==0||b==0||c1==0){serial_puts(c,"ALLOC-FAIL\n");循环(1){__asm(0xF4);}}
    serial_puts(c,"[a]");serial_num(c,a);serial_putc(c,10);
    /* 2. 写读校验 */
    整 i=0;
    循环(i<100){*(字节*)(a+i)=i;i=i+1;}
    i=0;循环(i<100){若(*(字节*)(a+i)!=i){serial_puts(c,"WRITE-FAIL\n");循环(1){__asm(0xF4);}}i=i+1;}
    /* 3. 地址对齐 + 不重叠 */
    整 ok=1;
    若(a%8!=0||b%8!=0||c1%8!=0){ok=0;}
    若(a+116>b){ok=0;}  /* a 数据区不撞 b */
    若(b+216>c1){ok=0;}
    /* 4. 释放 b, 再分配应复用 */
    kfree(b);
    mem_stats(c);
    整 d=kmalloc(150);
    若(d!=b){serial_puts(c,"[reuse]");serial_num(c,d);serial_putc(c,10);ok=0;}
    否则{serial_puts(c,"[reuse-ok]\n");}
    /* 5. 释放 a, 大块分配 */
    kfree(a);
    整 e=kmalloc(500);
    若(e==0){ok=0;}
    mem_stats(c);
    /* 判定 */
    若(ok==1){serial_puts(c,"MEM-PASS\n");}否则{serial_puts(c,"MEM-FAIL\n");}
    循环(1){__asm(0xF4);}
}
