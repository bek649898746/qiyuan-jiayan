/* 甲言内核 — mem 模块 (0.2 模块化)
 * 裸机内存管理器: 固定堆区, 首次适配 + 拆块.
 * bin 模式零全局 → 堆区固定地址宏.
 */
#ifndef KERNEL_MEM_H
#define KERNEL_MEM_H

#include "srclib/kernel/serial.h"

#define MEM_HEAP 0x3000000
#define MEM_HEAP_SIZE 0x100000   /* 1MB */
#define MEM_ALIGN 8

/* 初始化: 单空闲块覆盖整个堆 */
空 mem_init(void){
    整 *h=(整*)MEM_HEAP;
    *(整*)(MEM_HEAP+0)=MEM_HEAP_SIZE-16;
    *(整*)(MEM_HEAP+4)=0;
    *(整*)(MEM_HEAP+8)=0;
    *(整*)(MEM_HEAP+12)=0;
    *(整*)(MEM_HEAP+16)=0;
}
/* 分配: 首次适配, 返数据地址 (0=失败) */
整 kmalloc(整 size){
    若(size<=0)返 0;
    整 want=size+16;
    want=(want+7)&~7;
    整 addr=MEM_HEAP;
    循环(1){
        整 bsz=*(整*)addr;
        整 used=*(整*)(addr+4);
        若(bsz==0)断;
        若(used==0 && bsz>=want){
            若(bsz >= want+16){
                *(整*)(addr+0)=want;
                *(整*)(addr+4)=1;
                整 new=addr+want;
                *(整*)(new+0)=bsz-want;
                *(整*)(new+4)=0;
                返 addr+16;
            }否则{
                *(整*)(addr+4)=1;
                返 addr+16;
            }
        }
        addr=addr+bsz;
    }
    返 0;
}
/* 释放: 标记空闲 */
空 kfree(整 p){
    若(p==0)返;
    整 h=p-16;
    *(整*)(h+4)=0;
}
/* 统计: 块数/总空闲/最大空闲 */
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

#endif /* KERNEL_MEM_H */
