# -*- coding: utf-8 -*-
"""自宿主 H2 门禁: 自宿主编译器 v4 的 -S 文本 → asm_zh 汇编 → 产物 vs v4 直发 SHA 对比.
验证"双链在自宿主层面闭环" (host H2 门禁已绿, 此脚本把 -S 源从宿主换成自宿主 v4)."""
import subprocess, os, sys, hashlib, io, time
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
TDIR = os.path.join('build', 'h2_self')
os.makedirs(TDIR, exist_ok=True)

QCC = r'build\conv\v4.exe'  # 自宿主编译器 (5 代收敛 02142a5d)

files = []
for d in ('tests/qcc', 'tests/behavior'):
    for f in sorted(os.listdir(d)):
        if f.endswith('.c'):
            files.append(os.path.join(d, f))

def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()[:16] if os.path.exists(p) else None

agree = 0
diffs = []
skip = 0
t0 = time.time()
for i, src in enumerate(files):
    tag = os.path.basename(src)
    head = open(src, encoding='utf-8', errors='replace').read(400)
    if '@EXPECTED compile_fail' in head:
        skip += 1
        continue
    base = os.path.join(TDIR, f'f{i}')
    r = subprocess.run([QCC, '-S', src, '-o', base + '.asm'], capture_output=True, timeout=300)
    if r.returncode != 0 or not os.path.exists(base + '.asm.asm'):
        diffs.append(f'{tag}: v4 -S 失败 rc={r.returncode}')
        for p in (base + '.asm', base + '.asm.asm'):
            try: os.remove(p)
            except OSError: pass
        continue
    r2 = subprocess.run(['asm_zh.exe', base + '.asm.asm', '-o', base + '_a.exe'], capture_output=True, timeout=120)
    if r2.returncode != 0 or not os.path.exists(base + '_a.exe'):
        diffs.append(f'{tag}: asm_zh 汇编失败')
        for p in (base + '.asm', base + '.asm.asm', base + '_a.exe'):
            try: os.remove(p)
            except OSError: pass
        continue
    sa, sb = sha(base + '.asm'), sha(base + '_a.exe')
    if sa == sb:
        agree += 1
    else:
        diffs.append(f'{tag}: v4直发={sa} asmzh={sb}')
    for p in (base + '.asm', base + '.asm.asm', base + '_a.exe'):
        try: os.remove(p)
        except OSError: pass
    if (i + 1) % 30 == 0:
        print(f'  {i+1}/{len(files)} ... (一致 {agree}, 差异 {len(diffs)})', flush=True)

el = time.time() - t0
print(f'自宿主 H2 全量对等: 共 {len(files)} | 一致 {agree} | 跳过 {skip} | 差异 {len(diffs)} | 耗时 {el/60:.0f}min')
for d in diffs[:30]:
    print(' ', d)
