# -*- coding: utf-8 -*-
"""二分: 编译一次 vs 每次都编译, 对运行结果的影响."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
exp = '10\n3000000000\n3.14\n15 1705032704'

def runonce(h1):
    p = subprocess.run(['cmd', '/c', h1], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=15)
    return (p.stdout or '').strip() == exp

# A) 编译一次, 跑 10 次
subprocess.run(['qcc_x86.exe', 'tests/behavior/b_global.c', '-o', 'scratch_test/bg_a.exe'], capture_output=True)
ra = sum(1 for _ in range(10) if runonce('scratch_test/bg_a.exe'))
print(f'A) compile-once run-10:   ok={ra}/10')

# B) 每次编译后跑
rb = 0
for i in range(10):
    subprocess.run(['qcc_x86.exe', 'tests/behavior/b_global.c', '-o', 'scratch_test/bg_b.exe'], capture_output=True)
    if runonce('scratch_test/bg_b.exe'):
        rb += 1
print(f'B) compile-each run:      ok={rb}/10')

# C) 每次编译, 但运行前 sleep 0.3
rc = 0
for i in range(10):
    subprocess.run(['qcc_x86.exe', 'tests/behavior/b_global.c', '-o', 'scratch_test/bg_c.exe'], capture_output=True)
    import time; time.sleep(0.3)
    if runonce('scratch_test/bg_c.exe'):
        rc += 1
print(f'C) compile+0.3s-sleep run: ok={rc}/10')

# D) 拷贝新文件到另一个名字运行
rd = 0
for i in range(10):
    subprocess.run(['qcc_x86.exe', 'tests/behavior/b_global.c', '-o', 'scratch_test/bg_d.exe'], capture_output=True)
    import shutil
    shutil.copyfile('scratch_test/bg_d.exe', 'scratch_test/bg_d2.exe')
    if runonce('scratch_test/bg_d2.exe'):
        rd += 1
print(f'D) compile+copy-name run:  ok={rd}/10')
