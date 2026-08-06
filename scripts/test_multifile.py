# -*- coding: utf-8 -*-
"""甲言 多 .o 链接验收 (Task 5.1)
用法: python scripts/test_multifile.py
场景: qcc -c 产生 .o → jyld 链接多 .o → 运行验证
  1. 跨文件函数调用
  2. 跨文件全局变量 (extern) + 常量初始值 (.data 段)
  3. 三个 .o 链式调用
"""
import sys, os, subprocess, tempfile
sys.stdout.reconfigure(encoding='utf-8')
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(root)
os.makedirs('scratch_test', exist_ok=True)

# 确保 jyld
if not os.path.exists('jyld.exe'):
    r = subprocess.run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/jyld.c', '-o', 'jyld.exe'])
    assert r.returncode == 0

pass_n = fail_n = 0
fails = []

def check(name, srcs, expect_out):
    global pass_n, fail_n
    objs = []
    for i, src in enumerate(srcs):
        f = f'scratch_test/mf_{i}.c'
        open(f, 'w', encoding='utf-8').write(src)
        o = f'scratch_test/mf_{i}.o'
        r = subprocess.run(['qcc_x86.exe', '-c', f, '-o', o], capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(o):
            fail_n += 1; fails.append(f'{name}: {i}.c 编译失败'); return
        objs.append(o)
    exe = 'scratch_test/mf_app.exe'
    r = subprocess.run(['jyld.exe'] + objs + ['-o', exe], capture_output=True, text=True)
    if r.returncode != 0:
        fail_n += 1; fails.append(f'{name}: jyld 链接失败: {r.stderr[:200]}'); return
    r = subprocess.run([exe], capture_output=True, text=True, timeout=15)
    out = (r.stdout or '').strip()
    if out == expect_out:
        pass_n += 1
        print(f'[PASS] {name}')
    else:
        fail_n += 1; fails.append(f'{name}: 期望 {expect_out!r} 实际 {out!r}')
        print(f'[FAIL] {name}: 期望 {expect_out!r} 实际 {out!r}')

# 1. 跨文件函数调用
check('跨文件函数', [
    'int add(int x, int y) { return x + y; }\n',
    '#include <stdio.h>\nint add(int x, int y);\nint main(void) { printf("%d\\n", add(2, 3)); return 0; }\n',
], '5')

# 2. 跨文件全局变量 + 常量初始值
check('extern 全局+初始值', [
    'int counter = 100;\nint inc(int x) { counter += x; return counter; }\n',
    '#include <stdio.h>\nextern int counter;\nint inc(int x);\nint main(void) { printf("%d %d %d\\n", counter, inc(5), counter); return 0; }\n',
], '100 105 105')

# 3. 三文件链式调用
check('三文件链式', [
    'int triple(int x) { return x * 3; }\n',
    'int triple(int x);\nint quad(int x) { return triple(x) + x; }\n',
    '#include <stdio.h>\nint quad(int x);\nint main(void) { printf("%d\\n", quad(7)); return 0; }\n',
], '28')

# 4. 未初始化全局 (bss 清零)
check('未初始化全局', [
    'int g;\nvoid setg(int v) { g = v; }\n',
    '#include <stdio.h>\nextern int g;\nvoid setg(int v);\nint main(void) { printf("%d\\n", g); setg(42); printf("%d\\n", g); return 0; }\n',
], '0\n42')

print(f'\n多 .o 链接验收: PASS={pass_n} FAIL={fail_n}')
if fails:
    for f in fails:
        print('  ', f)
    sys.exit(1)
print('==== 全绿 ====')
