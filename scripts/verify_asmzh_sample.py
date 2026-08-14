# -*- coding: utf-8 -*-
"""抽样自宿主对齐: host asm_zh vs v2 asm_zh 产物 SHA 对比."""
import subprocess, os, sys, hashlib, io, time
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
TDIR = os.path.join('build', 'szh')
os.makedirs(TDIR, exist_ok=True)

sample = [
    'tests/behavior/b_scanf.c',
    'tests/behavior/b_bitfield.c',
    'tests/behavior/b_float.c',
    'tests/behavior/b_double.c',
    'tests/qcc/dbl_literals_test.c',
    'tests/qcc/math_call_test.c',
    'tests/qcc/regress_deep_static_bigparam.c',
    'tests/qcc/regress_ptr_step.c',
    'tests/qcc/regress_ptr_inc.c',
    'tests/qcc/regress_static_struct_assign.c',
    'tests/qcc/big_arr.c',
    'tests/qcc/arr_nested.c',
    'tests/qcc/struct_field.c',
    'tests/qcc/struct_assign.c',
    'tests/qcc/char_ptr_es.c',
    'tests/qcc/frame80.c',
    'tests/qcc/file_io.c',
    'tests/qcc/fprintf_test.c',
    'tests/qcc/str_lit.c',
    'tests/qcc/sprintf_test.c',
    'tests/qcc/unsigned_test.c',
    'tests/qcc/ll_ops.c',
    'tests/qcc/compound_literal.c',
    'tests/qcc/static_init.c',
    'tests/qcc/fib_combo.c',
    'tests/qcc/hello.c',
]

def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()[:16] if os.path.exists(p) else None

agree = 0
diffs = []
t0 = time.time()
for i, src in enumerate(sample):
    base = os.path.join(TDIR, f's{i}')
    subprocess.run(['qcc_x86.exe', '-S', src, '-o', base + '.asm'], capture_output=True, timeout=120)
    if not os.path.exists(base + '.asm.asm'):
        diffs.append(f'{os.path.basename(src)}: qcc -S 无文本')
        continue
    r1 = subprocess.run(['asm_zh.exe', base + '.asm.asm', '-o', base + '_h.exe'], capture_output=True, timeout=120)
    r2 = subprocess.run(['build/asm_zh_v2.exe', base + '.asm.asm', '-o', base + '_v.exe'], capture_output=True, timeout=600)
    if r1.returncode != 0 or not os.path.exists(base + '_h.exe'):
        diffs.append(f'{os.path.basename(src)}: host 汇编失败 rc={r1.returncode}')
    elif r2.returncode != 0 or not os.path.exists(base + '_v.exe'):
        diffs.append(f'{os.path.basename(src)}: v2 汇编失败 rc={r2.returncode} {(r2.stdout+r2.stderr).decode("utf-8","replace")[-100:]}')
    else:
        hh, hv = sha(base + '_h.exe'), sha(base + '_v.exe')
        if hh == hv:
            agree += 1
            print(f'  [{i+1}/{len(sample)}] {os.path.basename(src)}: MATCH', flush=True)
        else:
            diffs.append(f'{os.path.basename(src)}: host={hh} v2={hv}')
    for p in (base + '.asm', base + '.asm.asm', base + '_h.exe', base + '_v.exe'):
        try: os.remove(p)
        except OSError: pass

print(f'抽样自宿主对齐: 一致 {agree}/{len(sample)} | 耗时 {time.time()-t0:.0f}s')
for d in diffs:
    print(' ', d)
