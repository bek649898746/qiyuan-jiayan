// v58 — bin 模式 printf→串口验证 (Gate 9 桥头堡块1)
#include "srclib/kernel/serial.h"

空 _start(空){
    整 c=0x3F8;
    serial_init(c);
    serial_puts(c,"[serial]\n");
    printf("v58 printf test %d %x\n", 42, 0x2a);
    serial_puts(c,"[done]\n");
    循环(1){__asm(0xF4);}
}
