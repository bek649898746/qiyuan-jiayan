# -*- coding: utf-8 -*-
"""Phase 3 直译全等验证脚本
用法: python _ir_parity.py <ir_compiler.exe>
用 IR 版编译器编译 qcc_work.jy, 与基准不动点 SHA 对比.
退出码: 0=全等, 1=差异
"""
import sys, os, subprocess, hashlib, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
root = os.getcwd()
os.chdir(root)

BASELINE = 'F3ED6063'  # 当前不动点 (IR 上主线后, 2026-08-24 683b4fa)
# 口径: 本脚本 = 自举回归哨兵 (当前编译器编译当前源 == 当前不动点);
# 直译全等 (新旧编译器对同一旧源产物一致) 见施工日志 20260824_phase3_ir_mainline.md
ir_exe = sys.argv[1] if len(sys.argv) > 1 else 'scratch_test/ir_exp.exe'
src = 'srclib_jiayan/qcc_work.jy'
out = 'scratch_test/_ir_parity_out.exe'

# 编译
r = subprocess.run([os.path.join(root, ir_exe), src, '-o', out], capture_output=True, timeout=300)
if r.returncode != 0 or not os.path.exists(out):
    print(f'[FAIL] IR 编译器编译失败 rc={r.returncode}')
    print((r.stdout + r.stderr).decode('utf-8', 'replace')[-500:])
    sys.exit(1)

# SHA
h = hashlib.sha256(open(out, 'rb').read()).hexdigest()[:8]
print(f'IR 产物 SHA: {h}')
if h.upper() == BASELINE:
    print('[PASS] 直译全等: IR 产物 == 基准不动点')
    sys.exit(0)
else:
    print(f'[FAIL] 直译差异: {h} != 基准 {BASELINE}')
    sys.exit(1)
