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

# ─────────────────────────────────────────────────────────────────────────────
# 2026-10-22 启元（审计）：不再硬编码英文链 qcc_x86.exe。
#
#   英文链已于 jiayan-v1.0-stable（2026-08-24）封存 —— C 版 qcc_x86.c 原样留在该标签里
#   作为历史信任根，但不再进 CI、不再维护（seed 切到自举不动点编译器后，它已无法
#   编译当前规模的中文源，会死循环）。
#
#   本脚本原写死 qcc_x86.exe ⟹ 倒逼 CI 的 tests / nightly_full 每次都要 gcc 现编
#   一个用不到的英文链（每次白烧约 60 秒）。
#
#   现改为：默认用仓库自带的中文自举种子，允许 QCC_TEST_BIN 覆盖（与 run_tests_zh.py 同口径）。
#   ⟹ CI 不再需要 gcc 编英文链，且验证的是真正在用的链。
# ─────────────────────────────────────────────────────────────────────────────
QCC = os.environ.get('QCC_TEST_BIN') or 'qcc_zh_seed.exe'

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
        r = subprocess.run([QCC, '-c', f, '-o', o], capture_output=True, text=True, encoding='utf-8', errors='replace')
        if r.returncode != 0 or not os.path.exists(o):
            fail_n += 1; fails.append(f'{name}: {i}.c 编译失败'); return
        objs.append(o)
    exe = 'scratch_test/mf_app.exe'
    r = subprocess.run(['jyld.exe'] + objs + ['-o', exe], capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0:
        fail_n += 1; fails.append(f'{name}: jyld 链接失败: {r.stderr[:200]}'); return
    r = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=15)
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

# ---- Task 5.2: 多文件直接编译（原 jycc C 驱动已退役）----
# 2026-10-22 启元（审计）：jycc.c 是 C 写的驱动，内部硬编码 "qcc_x86.exe -c ..."（英文链组件）。
#   英文链已于 jiayan-v1.0-stable（2026-08-24）封存 ⟹ jycc 无编译器可调，必然 rc=1。
#   实测甲言编译器**本身就能多文件编译**：
#     qcc_zh_seed.exe -o out.exe lib.jy main.jy  ⟹ rc=0，运行输出 [5] 正确
#   ⟹ 直接调 QCC，**功能点（多文件编译）一个不丢**，C 驱动不再维护。
print('\n=== Task 5.2: 多文件直接编译（甲言编译器）===')
os.makedirs('scratch_test', exist_ok=True)
if not os.path.exists('jyld.exe'):
    r = subprocess.run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/jyld.c', '-o', 'jyld.exe'])
    assert r.returncode == 0

def jycc_case(name, files, expect_out):
    """多文件编译用例（名字沿用 jycc_case，减少调用处改动）。"""
    global pass_n, fail_n
    exe = 'scratch_test/jycc_app.exe'
    if os.path.exists(exe):
        try: os.remove(exe)
        except OSError: pass
    r = subprocess.run([QCC, '-o', exe] + files, capture_output=True, timeout=60)
    if r.returncode != 0 or not os.path.exists(exe):
        fail_n += 1; fails.append(f'{name}: 多文件编译失败 rc={r.returncode}')
        return
    r = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=15)
    out = (r.stdout or '').strip()
    if out == expect_out:
        pass_n += 1
        print(f'[PASS] {name}')
    else:
        fail_n += 1; fails.append(f'{name}: 期望 {expect_out!r} 实际 {out!r}')

# 甲言 .jy 多文件
open('scratch_test/jycc_lib.jy', 'w', encoding='utf-8').write('整 加(整 a, 整 b) { 返 a + b; }\n')
open('scratch_test/jycc_main.jy', 'w', encoding='utf-8').write('#include <stdio.h>\n整 加(整 a, 整 b);\n整 主() { printf("%d\\n", 加(2, 3)); 返 0; }\n')
jycc_case('jycc 甲言 .jy 多文件', ['scratch_test/jycc_lib.jy', 'scratch_test/jycc_main.jy'], '5')

# C 多文件
open('scratch_test/jycc_p1.c', 'w').write('int f1(void) { return 11; }\n')
open('scratch_test/jycc_p2.c', 'w').write('int f2(void) { return 22; }\n')
open('scratch_test/jycc_pm.c', 'w').write('#include <stdio.h>\nint f1(void);\nint f2(void);\nint main(void) { printf("%d\\n", f1() + f2()); return 0; }\n')
jycc_case('jycc C 多文件', ['scratch_test/jycc_p1.c', 'scratch_test/jycc_p2.c', 'scratch_test/jycc_pm.c'], '33')

# 清理 jycc 生成的 .o
import glob
for f in set(glob.glob('jycc_*.o') + glob.glob('*.o')):
    base = os.path.basename(f)
    if base.startswith('jycc_') or base in ('p1.o', 'p2.o', 'pmain.o', 'lib.o', 'mymain.o'):
        try:
            os.remove(f)
        except OSError:
            pass

print(f'\n总计 (多 .o 4 + jycc 2): PASS={pass_n} FAIL={fail_n}')
if fails:
    for f in fails:
        print('  ', f)
    sys.exit(1)
print('==== 全绿 ====')
