# -*- coding: utf-8 -*-
"""自宿主 H2 小样本: v4 自宿主编译器 -S → host asm_zh 汇编 → vs v4 直发 SHA."""
import subprocess, os, sys, hashlib, io, time
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
TDIR = 'scratch_test/_h2s'
os.makedirs(TDIR, exist_ok=True)
QCC = r'build\conv\v4.exe'

sample = [
    'tests/behavior/b_scanf.c',
    'tests/behavior/b_bitfield.c',
    'tests/qcc/math_call_test.c',
    'tests/qcc/bin_test.c',
    'tests/qcc/regress_deep_static_bigparam.c',
    'tests/behavior/b_struct.c',
]

def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()[:16] if os.path.exists(p) else None

t0 = time.time()
for i, src in enumerate(sample):
    base = os.path.join(TDIR, f's{i}')
    r = subprocess.run([QCC, '-S', src, '-o', base + '.asm'], capture_output=True, timeout=300)
    if r.returncode != 0 or not os.path.exists(base + '.asm.asm'):
        print(f'  {os.path.basename(src)}: v4 -S 失败 rc={r.returncode}', flush=True)
        continue
    r2 = subprocess.run(['asm_zh.exe', base + '.asm.asm', '-o', base + '_a.exe'], capture_output=True, timeout=120)
    if r2.returncode != 0 or not os.path.exists(base + '_a.exe'):
        print(f'  {os.path.basename(src)}: asm_zh 汇编失败', flush=True)
        continue
    sa, sb = sha(base + '.asm'), sha(base + '_a.exe')
    tag = 'MATCH' if sa == sb else f'DIFF {sa} vs {sb}'
    print(f'  [{i+1}/{len(sample)}] {os.path.basename(src)}: {tag}', flush=True)
    for p in (base + '.asm', base + '.asm.asm', base + '_a.exe'):
        try: os.remove(p)
        except OSError: pass

print(f'自宿主 H2 小样本完成, 耗时 {time.time()-t0:.0f}s')
