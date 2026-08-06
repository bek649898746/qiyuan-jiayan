# -*- coding: utf-8 -*-
# 行为正确性测试：编译 + 运行 + 断言 stdout == .expected（防"编译过但全错"）
# 用法: python tests/behavior/run_behavior.py [qcc路径]
import subprocess, io, os, sys, glob

BASE = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
os.chdir(BASE)
QCC = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else 'qcc_x86.exe')
BDIR = 'tests/behavior'

cases = sorted(glob.glob(os.path.join(BDIR, 'b_*.c')))
pass_n = fail_n = 0
fails = []
for src in cases:
    name = os.path.basename(src)[:-2]
    exp = os.path.join(BDIR, name + '.expected')
    exe = '_bt.exe'
    try:
        if os.path.exists(exe): os.remove(exe)
    except OSError: pass
    r = subprocess.run([QCC, src, '-o', exe], capture_output=True, timeout=30)
    if r.returncode != 0 or not os.path.exists(exe):
        fail_n += 1
        fails.append('%s: 编译失败 rc=%d %s' % (name, r.returncode, r.stderr.decode('utf-8','replace')[:80]))
        continue
    r2 = subprocess.run(['./' + exe], capture_output=True, timeout=30)
    out = r2.stdout.decode('utf-8', 'replace')
    expect = io.open(exp, encoding='utf-8').read()
    # 规范化行尾
    out_n = out.replace('\r\n', '\n').replace('\r', '\n')
    if out_n == expect:
        pass_n += 1
    else:
        fail_n += 1
        fails.append('%s: 输出不匹配 (rc=%d)\n  期望: %r\n  实际: %r' % (name, r2.returncode, expect[:120], out_n[:120]))
    try: os.remove(exe)
    except OSError: pass

print('== 行为测试: PASS %d FAIL %d (%s) ==' % (pass_n, fail_n, QCC))
for f in fails:
    print('FAIL', f)
