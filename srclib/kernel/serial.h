/* 甲言内核 — serial 模块 (Gate 5/0.2 模块化)
 * 唯一真实现: 8250 UART 串口. 所有探针的 si/sw/spc/sps/hex 副本退役.
 * bin 模式零全局 → 端口号显式传递 (首参数 c).
 */
#ifndef KERNEL_SERIAL_H
#define KERNEL_SERIAL_H

/* 初始化 16550 串口 (COM1 0x3F8) */
空 serial_init(整 c){
    outb(c+1,0);   /* IER=0 关中断 */
    outb(c+3,0x80);/* 设置 DLAB */
    outb(c+0,1);   /* 波特率 115200 (divisor=1) */
    outb(c+1,0);
    outb(c+3,3);   /* 8N1, DLAB off */
    outb(c+2,0xC7);/* FIFO 使能 */
    outb(c+4,0x0B);/* DTR/RTS/OUT2 */
}
/* 等发送缓冲空 (LSR bit5) */
空 serial_sw(整 c){ 循环((inb(c+5)&0x20)==0){} }
/* 输出单字符 (换行自动补 \r) */
空 serial_putc(整 c, 整 v){
    若(v==10){serial_sw(c);outb(c,13);}
    serial_sw(c);outb(c,v);
}
/* 输出字符串 */
空 serial_puts(整 c, 字 *s){ 循环(*s!=0){serial_putc(c,*s);s++;} }
/* 输出非负整数 (十进制) */
空 serial_num(整 c, 整 n){
    若(n>=1000){serial_putc(c,48+n/1000);n=n-(n/1000)*1000;若(n<100){serial_putc(c,48);}}
    若(n>=100){serial_putc(c,48+n/100);n=n-(n/100)*100;若(n<10){serial_putc(c,48);}}
    若(n>=10){serial_putc(c,48+n/10);n=n-(n/10)*10;}
    serial_putc(c,48+n);
}
/* 输出 4-bit 十六进制 (带前导0: "0X") */
空 serial_hexb(整 c, 整 v){
    serial_putc(c,48+((v>>4)&15));
    serial_putc(c,48+(v&15));
}
/* 输出 32-bit 十六进制 */
空 serial_hex32(整 c, 整 v){
    serial_hexb(c,(v>>28)&15);serial_hexb(c,(v>>24)&15);
    serial_hexb(c,(v>>20)&15);serial_hexb(c,(v>>16)&15);
    serial_hexb(c,(v>>12)&15);serial_hexb(c,(v>>8)&15);
    serial_hexb(c,(v>>4)&15);serial_hexb(c,v&15);
}

#endif /* KERNEL_SERIAL_H */
