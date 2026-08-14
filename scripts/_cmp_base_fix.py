# -*- coding: utf-8 -*-
"""基线 vs 修复版宿主对比失败测试."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\scratch_test')

def run_test(compiler, name):
    src = os.path.join('..', 'tests', 'behavior' if name == 'b_global.c' else 'qcc', name)
    r = subprocess.run([compiler, src, '-o', 'tt.exe'], capture_output=True)
    if r.returncode != 0:
        return f'compile rc={r.returncode}'
    p = subprocess.run(['cmd', '/c', 'tt.exe'], capture_output=True, text=True, timeout=15)
    return f'exit={p.returncode} out={p.stdout.strip()[:80]!r}'

tests = ['regress_compound_struct.c', 'regress_desig_idx2.c', 'regress_enum_dim.c',
         'regress_multi_nest.c', 'regress_multi_read.c', 'b_global.c']
print('=== BASE (411a3c1) ===')
for t in tests:
    print(f'  {t}: {run_test("../qcc_x86_base.exe", t)}')
print('=== FIX (data_extent) ===')
for t in tests:
    print(f'  {t}: {run_test("../qcc_x86.exe", t)}')
