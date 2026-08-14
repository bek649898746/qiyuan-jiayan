# -*- coding: utf-8 -*-
"""在 cg() 前 (5802) 插入 asm 编码器 — 修正插入点"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

# 从之前的脚本读取编码器文本
exec_src = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\scripts\_asm_encoder.py', encoding='utf-8').read()
# 提取 encoder 变量 (粗暴: 从 '/* fix 2026-08-11 汇编本身' 到 'fprintf(stderr, "[ERR] __asm 未编码'
start = exec_src.find("encoder = '''")
end = exec_src.find("'''", start + 12)
encoder = exec_src[start+12:end]

# 但脚本已运行一次 (插入到错误位置). 先回滚: git checkout
import subprocess, os
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
subprocess.run(['git','checkout','--','srclib/qcc_x86.c'])

# 在 cg() 前插入
p = 'srclib/qcc_x86.c'
t = open(p, 'rb').read().decode('utf-8', errors='replace')
anchor = 'static void cg(int n) {'
idx = t.find(anchor)
if idx > 0:
    t = t[:idx] + encoder + '\n\n' + t[idx:]
    open(p, 'wb').write(t.encode('utf-8'))
    print('[OK] 编码器已插到 cg() 前 (5802)')
else:
    print('[FAIL] cg 签名未找到')
