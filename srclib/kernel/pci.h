/* 甲言内核 — pci 模块 (0.2 模块化)
 * PCI 配置空间读写 + NVMe 控制器发现.
 * 唯一真实现, 探针内联副本退役.
 */
#ifndef KERNEL_PCI_H
#define KERNEL_PCI_H

/* 读 PCI 配置 32-bit (bus/dev/func/off) */
整 pci_read32(整 bus, 整 dev, 整 func, 整 off){
    整 a=(1<<31)|(bus<<16)|(dev<<11)|(func<<8)|(off&0xFC);
    outl(0xCF8,a);
    返 inl(0xCFC);
}
/* 写 PCI 配置 32-bit */
空 pci_write32(整 bus, 整 dev, 整 func, 整 off, 整 v){
    整 a=(1<<31)|(bus<<16)|(dev<<11)|(func<<8)|(off&0xFC);
    outl(0xCF8,a);
    outl(0xCFC,v);
}
/* 扫设备 0-31, 找类 0x0108 (NVMe), 返设备号 (-1 未找到) */
整 pci_find_nvme(void){
    整 dev=0;
    循环(dev<32){
        整 vid=pci_read32(0,dev,0,0);
        若(vid!=0xFFFFFFFF && vid!=0){
            整 cls=(pci_read32(0,dev,0,8)>>16)&0xFFFF;
            若(cls==0x0108){返 dev;}
        }
        dev=dev+1;
    }
    返 -1;
}
/* 取 BAR0 (MMIO 基址, 去位标记) */
整 pci_bar0(整 dev){
    返 pci_read32(0,dev,0,0x10)&~15;
}

#endif /* KERNEL_PCI_H */
