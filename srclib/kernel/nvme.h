/* 甲言内核 — nvme 模块 (0.2 模块化)
 * NVMe 控制器驱动: 初始化 + Identify + I/O 队列读写.
 * bin 模式零全局 → 上下文结构显式传递.
 */
#ifndef KERNEL_NVME_H
#define KERNEL_NVME_H

#include "srclib/kernel/serial.h"
#include "srclib/kernel/pci.h"

/* NVMe 上下文 (状态全在此, 无全局) */
typedef struct {
    整 mmio;    /* BAR0 MMIO 基址 */
    整 asq;     /* admin SQ */
    整 acq;     /* admin CQ */
    整 iosq;    /* I/O SQ */
    整 iocq;    /* I/O CQ */
    整 io_n;    /* I/O 命令累计数 (槽位/tail/head 基准) */
    整 cmd;     /* admin 命令累计数 */
} NvmeCtx;

/* 找 NVMe 设备并填上下文, 返 0=成功 */
整 nvme_find(NvmeCtx *ctx){
    整 dev=pci_find_nvme();
    若(dev<0)返 -1;
    ctx->mmio=pci_bar0(dev);
    ctx->asq=0x7FDD000;
    ctx->acq=0x7FDE000;
    ctx->iosq=0x331000;
    ctx->iocq=0x330000;
    ctx->io_n=0;
    ctx->cmd=0;
    返 0;
}

/* 等 admin 完成 (CID=槽位+1, QEMU 10.2) */
整 nvme_waitcq(整 cqbase, 整 entries, 整 cid){
    整 t=0;
    循环(t<20000){
        整 i=0;
        循环(i<entries){
            整 dw2=*(整*)(cqbase+i*16+8);
            若((dw2&0xFFFF)==cid){返 *(整*)(cqbase+i*16+12);}
            i=i+1;
        }
        t=t+1;
    }
    返 -1;
}
/* admin 命令 */
整 nvme_acmd(NvmeCtx *ctx, 整 op, 整 cid, 整 prp1, 整 cdw10, 整 cdw11){
    整 *sq=(整*)ctx->asq;
    整 slot=ctx->cmd*16;
    sq[slot+0]=op|(cid<<16);sq[slot+1]=0;sq[slot+2]=0;sq[slot+3]=0;
    sq[slot+4]=0;sq[slot+5]=0;sq[slot+6]=prp1;sq[slot+7]=0;
    sq[slot+8]=0;sq[slot+9]=0;sq[slot+10]=cdw10;sq[slot+11]=cdw11;
    sq[slot+12]=0;sq[slot+13]=0;sq[slot+14]=0;sq[slot+15]=0;
    *(整*)(ctx->mmio+0x1000)=ctx->cmd+1;
    整 st=nvme_waitcq(ctx->acq,64,ctx->cmd+1);
    ctx->cmd=ctx->cmd+1;
    返 st;
}
/* I/O 读写: op=0x01 写 / 0x02 读 */
整 nvme_iorw(NvmeCtx *ctx, 整 op, 整 cid, 整 prp1, 整 slba, 整 nlb){
    整 *sq=(整*)ctx->iosq;
    整 slot=ctx->io_n&31;
    sq[slot*16+0]=op|(cid<<16);sq[slot*16+1]=1;sq[slot*16+2]=0;sq[slot*16+3]=0;
    sq[slot*16+4]=0;sq[slot*16+5]=0;sq[slot*16+6]=prp1;sq[slot*16+7]=0;
    sq[slot*16+8]=0;sq[slot*16+9]=0;sq[slot*16+10]=slba;sq[slot*16+11]=0;
    sq[slot*16+12]=nlb;sq[slot*16+13]=0;sq[slot*16+14]=0;sq[slot*16+15]=0;
    *(整*)(ctx->mmio+0x1008)=((ctx->io_n+1)&31);
    整 st=nvme_waitcq(ctx->iocq,32,slot+1);
    *(整*)(ctx->mmio+0x100C)=((ctx->io_n+1)&31);
    ctx->io_n=ctx->io_n+1;
    返 st;
}
/* 初始化控制器 (禁用→AQA→队列→启用RDY 重试) */
整 nvme_init(NvmeCtx *ctx, 整 buf){
    整 mmio=ctx->mmio;
    *(整*)(mmio+0x14)=0;
    整 t=0;
    循环((*(整*)(mmio+0x1C))&1){t=t+1;若(t>2000000){返 -1;}}
    /* 清零队列内存 */
    整 i=0;
    循环(i<1024){*(字节*)(ctx->acq+i)=0;i=i+1;}
    循环(i<17408){*(字节*)(ctx->asq+i-1024)=0;i=i+1;}
    循环(i<21504){*(字节*)(buf+i-17408)=0;i=i+1;}
    循环(i<22016){*(字节*)(ctx->iocq+i-21504)=0;i=i+1;}
    循环(i<24064){*(字节*)(ctx->iosq+i-22016)=0;i=i+1;}
    *(整*)(mmio+0x24)=0xFF003F;
    *(整*)(mmio+0x28)=ctx->asq;*(整*)(mmio+0x2C)=0;
    *(整*)(mmio+0x30)=ctx->acq;*(整*)(mmio+0x34)=0;
    /* 启用重试 */
    整 rtry=0; 整 rdy1=0;
    循环(rtry<5 && rdy1==0){
        *(整*)(mmio+0x14)=1|(6<<16)|(4<<20);
        t=0;
        循环((*(整*)(mmio+0x1C))&1==0){t=t+1;若(t>3000000){break;}}
        若((*(整*)(mmio+0x1C))&1){rdy1=1;}
        否则{*(整*)(mmio+0x14)=0;t=0;循环((*(整*)(mmio+0x1C))&1){t=t+1;若(t>1000000){break;}}}
        rtry=rtry+1;
    }
    若(rdy1==0)返 -2;
    /* Identify + Create CQ/SQ */
    整 st=nvme_acmd(ctx,0x06,0x41,buf,1,0);
    st=nvme_acmd(ctx,0x05,0x42,ctx->iocq,1|(31<<16),1);
    st=nvme_acmd(ctx,0x01,0x43,ctx->iosq,1|(31<<16),1|(1<<16));
    返 0;
}

#endif /* KERNEL_NVME_H */
