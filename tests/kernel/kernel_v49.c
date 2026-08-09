// v49 — 0.2 模块化验证: serial/pci/nvme/tensor 模块组合 (唯一真实现)
#include "srclib/kernel/serial.h"
#include "srclib/kernel/pci.h"
#include "srclib/kernel/nvme.h"
#include "srclib/kernel/tensor.h"

空 _start(空){
    整 c=0x3F8;
    serial_init(c);
    serial_puts(c,"v49\n");
    /* NVMe 上下文 + 初始化 */
    NvmeCtx nv;
    整 r=nvme_find(&nv);
    若(r!=0){serial_puts(c,"NF\n");循环(1){__asm(0xF4);}}
    serial_puts(c,"MMIO=");serial_hex32(c,nv.mmio);serial_putc(c,10);
    整 buf=0x310000;
    r=nvme_init(&nv,buf);
    若(r!=0){serial_puts(c,"INIT-FAIL\n");循环(1){__asm(0xF4);}}
    serial_puts(c,"SN=");
    serial_putc(c,*(字节*)(buf+4));serial_putc(c,*(字节*)(buf+5));
    serial_putc(c,*(字节*)(buf+6));serial_putc(c,*(字节*)(buf+7));
    serial_putc(c,10);
    /* Tensor 池 */
    整 tbl=TPOOL_BASE;
    整 i=0;
    循环(i<TENTRIES*52){*(字节*)(tbl+i)=0;i=i+1;}
    /* 清零磁盘池区 */
    整 pbuf=0x320000;
    i=0;循环(i<4096){*(字节*)(pbuf+i)=0;i=i+1;}
    nvme_iorw(&nv,0x01,0x70,pbuf,DISK_BASE_LBA,63);
    /* hello v1 "ABCDE" → 提交 */
    整 a=tp_put(tbl,"hello",5);
    i=0;循环(i<5){*(字节*)(a+i)=65+i;i=i+1;}
    tp_commit(&nv,tbl,"hello");
    serial_puts(c,"V1=");serial_num(c,tp_version(tbl,"hello"));serial_putc(c,10);
    /* hello v2 "KLMNO" → 提交 */
    整 b=tp_put(tbl,"hello",5);
    i=0;循环(i<5){*(字节*)(b+i)=75+i;i=i+1;}
    tp_commit(&nv,tbl,"hello");
    serial_puts(c,"V2=");serial_num(c,tp_version(tbl,"hello"));serial_putc(c,10);
    /* 取最新 → 磁盘读回 */
    整 out=0x320000;
    i=0;循环(i<4096){*(字节*)(out+i)=0;i=i+1;}
    tp_get(&nv,tbl,"hello",out);
    serial_puts(c,"G=");
    i=0;循环(i<5){serial_putc(c,*(字节*)(out+i));i=i+1;}
    serial_putc(c,10);
    /* 判定 */
    整 ok=1;
    若(tp_version(tbl,"hello")!=2){ok=0;}
    若(*(字节*)(out+0)!=75){ok=0;}
    若(ok==1){serial_puts(c,"MODULES-PASS\n");}否则{serial_puts(c,"MODULES-FAIL\n");}
    循环(1){__asm(0xF4);}
}
