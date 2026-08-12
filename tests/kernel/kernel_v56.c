// v56 — Gate5 收尾: Tensor 池版本链 GC (tp_gc 回收超限旧版本, 保留最新 4 版)
#include "srclib/kernel/serial.h"
#include "srclib/kernel/pci.h"
#include "srclib/kernel/nvme.h"
#include "srclib/kernel/tensor.h"

空 _start(空){
    整 c=0x3F8;
    serial_init(c);
    serial_puts(c,"v56\n");
    /* NVMe 初始化 */
    NvmeCtx nv;
    整 r=nvme_find(&nv);
    若(r!=0){serial_puts(c,"NF\n");循环(1){__asm(0xF4);}}
    整 buf=0x310000;
    r=nvme_init(&nv,buf);
    若(r!=0){serial_puts(c,"INIT-FAIL\n");循环(1){__asm(0xF4);}}
    /* Tensor 池 + 磁盘池区清零 */
    整 tbl=TPOOL_BASE;
    整 i=0;
    循环(i<TENTRIES*52){*(字节*)(tbl+i)=0;i=i+1;}
    整 pbuf=0x320000;
    i=0;循环(i<4096){*(字节*)(pbuf+i)=0;i=i+1;}
    nvme_iorw(&nv,0x01,0x70,pbuf,DISK_BASE_LBA,63);
    /* 创建 6 个版本 (v1..v6), 每版数据 = 起始字符 */
    整 v=1;
    循环(v<=6){
        整 a=tp_put(tbl,"data",5);
        整 k=0; 循环(k<5){*(字节*)(a+k)=64+v;k=k+1;}  /* A,B,C,D,E,F */
        tp_commit(&nv,tbl,"data");
        v=v+1;
    }
    serial_puts(c,"VN=");serial_num(c,tp_version(tbl,"data"));serial_putc(c,10);  /* 应为 6 */
    /* GC: 回收超限旧版本 (6-4=2 个最旧) */
    整 freed=tp_gc(tbl);
    serial_puts(c,"FREED=");serial_num(c,freed);serial_putc(c,10);  /* 应为 2 */
    /* 取最新 → 磁盘读回 */
    整 out=0x320000;
    i=0;循环(i<4096){*(字节*)(out+i)=0;i=i+1;}
    tp_get(&nv,tbl,"data",out);
    serial_puts(c,"G=");
    i=0;循环(i<5){serial_putc(c,*(字节*)(out+i));i=i+1;}
    serial_putc(c,10);  /* 应为 FFFFF (v6) */
    /* 判定: GC 回收 2, 最新版仍 v6 数据 */
    整 ok=1;
    若(freed!=2){ok=0;}
    若(tp_version(tbl,"data")!=6){ok=0;}
    若(*(字节*)(out+0)!=70){ok=0;}  /* 'F' = v6 */
    若(ok==1){serial_puts(c,"GC-PASS\n");}否则{serial_puts(c,"GC-FAIL\n");}
    循环(1){__asm(0xF4);}
}
