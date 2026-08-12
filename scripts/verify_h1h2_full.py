# -*- coding: utf-8 -*-
"""H1==H2 全量门禁: 所有测试文件, 宿主 vs v1 编译产物 SHA 逐一对比.

覆盖 Windows 模式 (tests/qcc + tests/behavior) 与 bin 模式 (tests/kernel).
compile_fail 测试 (宿主和 v1 都失败) 视为一致通过.
用法: python scripts/verify_h1h2_full.py [v1路径]
"""
import subprocess, os, re, sys, hashlib, shutil, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

ROOT = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
os.chdir(ROOT)
V1 = sys.argv[1] if len(sys.argv) > 1 else os.path.join('build', 'conv', 'v1.exe')
if not os.path.exists(V1):
    print(f'v1 不存在: {V1} — 请先跑 scripts/_bootstrap_conv.py'); sys.exit(1)

TDIR = os.path.join('build', 'h1h2')
shutil.rmtree(TDIR, ignore_errors=True)
os.makedirs(TDIR)

def collect():
    files = []
    for d in ('tests/qcc', 'tests/behavior'):
        for f in sorted(os.listdir(d)):
            if f.endswith('.c'):
                files.append((os.path.join(d, f), False))  # (path, is_bin)
    kdir = 'tests/kernel'
    if os.path.isdir(kdir):
        for f in sorted(os.listdir(kdir)):
            if f.endswith('.c'):
                files.append((os.path.join(kdir, f), True))
    return files

def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()[:16] if os.path.exists(p) else None

def compile_one(compiler, src, out, is_bin):
    cmd = [compiler, src, '-bin', '-o', out] if is_bin else [compiler, src, '-o', out]
    r = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=300)
    return r.returncode

files = collect()
diffs = []
agree = 0
cfail = 0
skip = 0
for i, (src, is_bin) in enumerate(files):
    tag = os.path.basename(src)
    head = open(src, encoding='utf-8', errors='replace').read(400)
    if '@EXPECTED compile_fail' in head:
        skip += 1  # 宿主/v1 都应编译失败 — 不对比产物 (编译失败本身由 run_tests 覆盖)
        continue
    oh = os.path.join(TDIR, f'h{i}.exe')
    ov = os.path.join(TDIR, f'v{i}.exe')
    rh = compile_one('qcc_x86.exe', src, oh, is_bin)
    rv = compile_one(V1, src, ov, is_bin)
    if rh != 0 or rv != 0:
        if rh == rv:
            cfail += 1  # 都失败 — 一致
        else:
            diffs.append(f'{tag}: 编译结果不一致 host_rc={rh} v1_rc={rv}')
        continue
    sh = sha(oh); sv = sha(ov)
    if sh == sv and sh is not None:
        agree += 1
    else:
        diffs.append(f'{tag}: host={sh} v1={sv}')
    # fix 2026-08-13: 逐文件删中间产物 (2×52MB/文件, 218 文件 ≈ 22GB, 防 C 盘堆积)
    for p in (oh, ov):
        try: os.remove(p)
        except OSError: pass

print(f'H1==H2 门禁: 共 {len(files)} 文件 | 产物一致 {agree} | 双失败一致 {cfail} | compile_fail 跳过 {skip} | 差异 {len(diffs)}')
if diffs:
    print('--- 差异列表 ---')
    for d in diffs[:40]:
        print(' ', d)
    sys.exit(1)
print('==== H1==H2 全量通过 ====')
