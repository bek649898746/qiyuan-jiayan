# -*- coding: utf-8 -*-
"""甲言 行为断言测试运行器 (Python 版)
用法: python scripts/run_tests.py
断言格式（测试 .c 头部注释）:
  // @EXPECTED exit:0           退出码 == 0
  // @EXPECTED exit:42          退出码 == 42
  // @EXPECTED exit:nonzero     退出码 != 0 (探针/崩溃)
  // @EXPECTED out:<str>        stdout 包含该字符串 (可选)
无 @EXPECTED 时默认 exit:0
"""
import sys, os, subprocess, re
sys.stdout.reconfigure(encoding='utf-8')

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(root)
os.makedirs('scratch_test', exist_ok=True)

# 1. 确保编译器
qcc = os.path.join(root, 'qcc_x86.exe')
if not os.path.exists(qcc):
    print('[1] 编译宿主 qcc_x86 ...')
    r = subprocess.run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/qcc_x86.c', '-o', qcc])
    if r.returncode != 0:
        print('[FAIL] 编译宿主失败'); sys.exit(1)

tests = sorted('qcc/' + f for f in os.listdir('tests/qcc') if f.endswith('.c'))
tests += sorted('behavior/' + f for f in os.listdir('tests/behavior') if f.endswith('.c'))
pass_n = fail_n = 0
fails = []

for name in tests:
    src = os.path.join('tests', name)  # name 已含 qcc/ 或 behavior/ 前缀
    # 读断言
    expected = '0'; expected_out = None
    try:
        head = open(src, encoding='utf-8', errors='replace').read(600)
    except Exception:
        head = ''
    for line in head.split('\n'):
        m = re.search(r'//\s*@EXPECTED\s+exit:(\S+)', line)
        if m: expected = m.group(1)
        m = re.search(r'//\s*@EXPECTED\s+out:(.+)$', line)
        if m: expected_out = m.group(1).strip()

    # H1 编译 (输出名用 basename, 兼容 behavior/ 子目录)
    h1 = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_H1.exe')
    try:
        os.remove(h1)
    except OSError:
        pass
    r = subprocess.run([qcc, src, '-o', h1], capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0 or not os.path.exists(h1):
        fail_n += 1; fails.append(f'{name} 编译失败'); continue

    # 运行
    out_file = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_out.txt')
    err_file = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_err.txt')
    try:
        os.remove(out_file); os.remove(err_file)
    except OSError:
        pass
    try:
        p = subprocess.run([h1], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=15)
        rc = p.returncode
        out = (p.stdout or '').strip()
    except subprocess.TimeoutExpired as te:
        # 超时后必须 kill: 崩溃/卡死进程残留会锁住 _H1.exe, 下次覆盖写失败 (fix 2026-08-08)
        try:
            te.kill()  # 杀整个进程组 (包含可能的子进程)
        except Exception:
            pass
        rc = 'TIMEOUT'; out = ''
    except Exception as e:
        rc = 'ERR'; out = str(e)
    try:
        os.remove(h1); os.remove(out_file); os.remove(err_file)
    except OSError:
        pass

    # 断言
    ok = False
    if expected == 'nonzero':
        ok = (rc != 0)
    else:
        ok = (str(rc) == expected)
    if ok and expected_out:
        ok = expected_out in out

    if ok:
        pass_n += 1
    else:
        fail_n += 1
        fails.append(f"{name} (期望exit={expected} 实际={rc} out='{out[:40]}')")

print(f'\n行为断言: PASS={pass_n} FAIL={fail_n}')
if fails:
    print('--- 失败 ---')
    for f in fails:
        print(f'  {f}')
    sys.exit(1)
print('==== 全绿 ====')
