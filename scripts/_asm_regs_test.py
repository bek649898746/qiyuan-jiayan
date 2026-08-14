# -*- coding: utf-8 -*-
"""测试寄存器/立即数指令编码"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

r = subprocess.run([r'.\qcc_x86.exe', '-bin', r'tests/kernel/regress_asm_regs.c', '-o', r'scratch_test\_regs.bin'],
                   capture_output=True, timeout=60)
print('编译 rc=%d' % r.returncode)
if r.returncode == 0:
    d = open(r'scratch_test\_regs.bin', 'rb').read()
    # 找 mov rax,0x10 = 48 C7 C0 10 00 00 00
    seq = bytes([0x48, 0xC7, 0xC0, 0x10, 0, 0, 0])
    idx = d.find(seq)
    if idx >= 0:
        print('mov rax,0x10 编码正确 @0x%X' % idx)
        print('字节:', ' '.join('%02X' % x for x in d[idx:idx+40]))
    else:
        print('未找到 mov rax,0x10 序列')
        # 打印可能的错误
        idx2 = d.find(b'\x48\xC7')
        if idx2 >= 0:
            print('48 C7 @0x%X:' % idx2, ' '.join('%02X' % x for x in d[idx2:idx2+10]))
else:
    print((r.stdout + r.stderr).decode('utf-8', errors='replace')[-200:])
