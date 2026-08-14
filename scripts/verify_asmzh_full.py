# -*- coding: utf-8 -*-
"""asm_zh.jy 全量自宿主对等: 每个 .c → qcc -S → host asm_zh vs v2 (qcc 编译 asm_zh.jy) 产物 SHA 对比.
逐文件处理 + 立即删中间文件防磁盘满."""
import subprocess, os, sys, hashlib, io, time
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
TDIR = os.path.join('build', 'szh_full')
os.makedirs(TDIR, exist_ok=True)

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
    r = subprocess.run(['qcc_x86.exe', '-S', src, '-o', base + '.asm'], capture_output=True, timeout=120)
    if r.returncode != 0 or not os.path.exists(base + '.asm.asm'):
        diffs.append(f'{tag}: qcc -S 失败 rc={r.returncode}')
        for p in (base + '.asm', base + '.asm.asm'):
            try: os.remove(p)
            except OSError: pass
        continue
    r1 = subprocess.run(['asm_zh.exe', base + '.asm.asm', '-o', base + '_h.exe'], capture_output=True, timeout=120)
    try:
        r2 = subprocess.run(['build/asm_zh_v2.exe', base + '.asm.asm', '-o', base + '_v.exe'], capture_output=True, timeout=900)
    except subprocess.TimeoutExpired:
        diffs.append(f'{tag}: v2 超时(>15min)')
        for p in (base + '.asm', base + '.asm.asm', base + '_h.exe', base + '_v.exe'):
            try: os.remove(p)
            except OSError: pass
        continue
    if r1.returncode != 0 or not os.path.exists(base + '_h.exe'):
        diffs.append(f'{tag}: host 汇编失败 rc={r1.returncode}')
    elif r2.returncode != 0 or not os.path.exists(base + '_v.exe'):
        diffs.append(f'{tag}: v2 汇编失败 rc={r2.returncode} {(r2.stdout+r2.stderr).decode("utf-8","replace")[-120:]}')
    else:
        hh, hv = sha(base + '_h.exe'), sha(base + '_v.exe')
        if hh == hv:
            agree += 1
        else:
            diffs.append(f'{tag}: host={hh} v2={hv}')
    for p in (base + '.asm', base + '.asm.asm', base + '_h.exe', base + '_v.exe'):
        try: os.remove(p)
        except OSError: pass
    if (i + 1) % 25 == 0:
        el = time.time() - t0
        print(f'  {i+1}/{len(files)} ... ({el/60:.0f}min, 一致 {agree}, 差异 {len(diffs)})', flush=True)

el = time.time() - t0
print(f'asm_zh 全量自宿主对等: 共 {len(files)} | 一致 {agree} | 跳过 {skip} | 差异 {len(diffs)} | 耗时 {el/60:.0f}min')
for d in diffs[:40]:
    print(' ', d)
