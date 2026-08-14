# -*- coding: utf-8 -*-
"""单测试 H2 diff: qcc -S → asm_zh → 对比, 显示 diff 位置."""
import subprocess, sys, io, os
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
src = sys.argv[1]
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
base = 'scratch_test/_hd'
for p in ('_hd.asm', '_hd.asm.asm', '_hd_a.exe'):
    try: os.remove(base + p[1:])
    except OSError: pass
r = subprocess.run(['qcc_x86.exe', '-S', src, '-o', base + '.asm'], capture_output=True, text=True)
r2 = subprocess.run(['asm_zh.exe', base + '.asm.asm', '-o', base + '_a.exe'], capture_output=True, text=True)
a = open(base + '.asm', 'rb').read()
b = open(base + '_a.exe', 'rb').read()
print(src, '| sizes', len(a), len(b), '| asm_zh rc', r2.returncode)
if r2.returncode != 0:
    print(r2.stderr[:300])
    sys.exit(0)
diffs = [(i, a[i], b[i]) for i in range(min(len(a), len(b))) if a[i] != b[i]]
print('diff 字节数:', len(diffs))
for i, av, bv in diffs[:15]:
    print('  0x%05x: 直发 %02x asm %02x' % (i, av, bv))
# 第一个 diff 的 .text 上下文 (文件 0x200 = .text 起点)
if diffs:
    d = diffs[0][0]
    off = max(0x200, d - 8)
    print('直发 [%x:%x]:' % (off, off + 24), a[off:off+24].hex())
    print('asm  [%x:%x]:' % (off, off + 24), b[off:off+24].hex())
