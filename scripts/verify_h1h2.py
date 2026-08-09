# -*- coding: utf-8 -*-
# 全量 H1==H2: tests/qcc 全部测试 host vs v2(镜像) 编译产物对比
# v2: 超时/挂死 = 失败
import subprocess, os, hashlib
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

def run(compiler, src, out, tmo=30):
    try:
        r = subprocess.run([compiler, src, '-o', out], capture_output=True, timeout=tmo)
        return r.returncode, None
    except subprocess.TimeoutExpired:
        return 'TIMEOUT', None

tests = sorted([f for f in os.listdir('tests/qcc') if f.endswith('.c')])
pass_n = 0
fail_n = 0
fail_list = []
for t in tests:
    rh, _ = run(r'.\qcc_x86.exe', r'tests\qcc\%s' % t, r'scratch_test\_h2_h.exe')
    rv, _ = run(r'.\v2.exe', r'tests\qcc\%s' % t, r'scratch_test\_h2_v.exe')
    if rh == 0 and rv == 0:
        hh = hashlib.sha256(open(r'scratch_test\_h2_h.exe','rb').read()).hexdigest()
        hv = hashlib.sha256(open(r'scratch_test\_h2_v.exe','rb').read()).hexdigest()
        if hh == hv:
            pass_n += 1
        else:
            fail_n += 1
            fail_list.append(t)
    elif rh != 0 and rv != 0 and rh != 'TIMEOUT' and rv != 'TIMEOUT':
        pass_n += 1  # 双方都编译失败 = 一致
    else:
        fail_n += 1
        fail_list.append(t + f' (host={rh} v2={rv})')
print(f'H1==H2: {pass_n}/{len(tests)} 通过')
if fail_list:
    print('FAIL:')
    for f in fail_list:
        print('  ', f)
