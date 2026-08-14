# -*- coding: utf-8 -*-
"""H2 全量对等: qcc -S → asm_zh 汇编 → 产物 vs 直发 SHA (逐文件处理, 立即删中间文件)."""
import subprocess, os, re, sys, hashlib, shutil, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

ROOT = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
os.chdir(ROOT)
TDIR = os.path.join('build', 'h2')
shutil.rmtree(TDIR, ignore_errors=True)
os.makedirs(TDIR)

files = []
for d in ('tests/qcc', 'tests/behavior'):
    for f in sorted(os.listdir(d)):
        if f.endswith('.c'):
            files.append(os.path.join(d, f))

def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()[:16] if os.path.exists(p) else None

agree = 0
cfail = 0
skip = 0
diffs = []
for i, src in enumerate(files):
    tag = os.path.basename(src)
    head = open(src, encoding='utf-8', errors='replace').read(400)
    if '@EXPECTED compile_fail' in head:
        skip += 1
        continue
    base = os.path.join(TDIR, f'f{i}')
    r = subprocess.run(['qcc_x86.exe', '-S', src, '-o', base + '.asm'], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=120)
    if r.returncode != 0 or not os.path.exists(base + '.asm.asm'):
        cfail += 1
        for p in (base + '.asm', base + '.asm.asm'):
            try: os.remove(p)
            except OSError: pass
        continue
    r2 = subprocess.run(['asm_zh.exe', base + '.asm.asm', '-o', base + '_a.exe'], capture_output=True,
                        text=True, encoding='utf-8', errors='replace', timeout=120)
    if r2.returncode != 0 or not os.path.exists(base + '_a.exe'):
        diffs.append(f'{tag}: asm_zh 汇编失败')
        for p in (base + '.asm', base + '.asm.asm', base + '_a.exe'):
            try: os.remove(p)
            except OSError: pass
        continue
    sa = sha(base + '.asm')
    sb = sha(base + '_a.exe')
    if sa == sb:
        agree += 1
    else:
        diffs.append(f'{tag}: host={sa} asmzh={sb}')
    for p in (base + '.asm', base + '.asm.asm', base + '_a.exe'):
        try: os.remove(p)
        except OSError: pass
    if (i + 1) % 30 == 0:
        print(f'  {i+1}/{len(files)} ...', flush=True)

print(f'H2 全量对等: 共 {len(files)} 文件 | 对等 {agree} | 双失败 {cfail} | 跳过 {skip} | 差异 {len(diffs)}')
if diffs:
    for d in diffs[:30]:
        print(' ', d)
    sys.exit(1)
print('==== H2 全量对等 ====')
