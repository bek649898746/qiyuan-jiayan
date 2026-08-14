# -*- coding: utf-8 -*-
"""测试 __asm 字符串汇编: bin 模式生成字节验证"""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

# bin 模式编译测试
r = subprocess.run([r'.\qcc_x86.exe', '-bin', r'tests/kernel/regress_asm_str.c', '-o', r'scratch_test\_asm.bin'],
                   capture_output=True, timeout=60)
print('编译 rc=%d' % r.returncode)
print((r.stdout + r.stderr).decode('utf-8', errors='replace')[-100:] if r.returncode != 0 else '')

if r.returncode == 0:
    d = open(r'scratch_test\_asm.bin', 'rb').read()
    print('bin 大小:', len(d))
    # 找 F4 90 C3 序列 (hlt nop ret)
    idx = d.find(b'\xF4\x90\xC3')
    if idx >= 0:
        print('找到 hlt nop ret 序列 @0x%X' % idx)
        print('字节:', ' '.join('%02X' % x for x in d[idx-4:idx+8]))
    else:
        # 找 F4 单独
        idx2 = d.find(b'\xF4')
        print('hlt (F4) @0x%X' % idx2 if idx2 >= 0 else '未找到 F4')
        # 打印开头
        print('开头:', ' '.join('%02X' % x for x in d[:32]))
