# ___chkstk_ms 最小存根: gcc 在函数 prologue 需要 >4KB 栈帧时调用
# 约定: rax = 需要的栈字节数 (gcc msvcrt 模式)。调整 rsp 即可。
.text
.globl ___chkstk_ms
___chkstk_ms:
    subq %rax, %rsp
    ret
