# -*- coding: utf-8 -*-
"""差异测试台: 半随机生成 C 程序, qcc vs gcc 编译+运行+比对输出
覆盖 bug 族: 类型尺寸 / 变参 / 布局敏感 / 常规运算 / 控制流
"""
import random, subprocess, os, sys, io, hashlib
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

ROOT = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
GITDIR = r'C:\Users\Administrator\Desktop\git-2.45.2'
PRELUDE = os.path.join(ROOT, 'compat_prelude.h')
QCC = os.path.join(ROOT, 'v1.exe')  # 当前自举编译器
QCC_ARGS = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1', '-I', PRELUDE]
OUT_DIR = r'D:\qiyuan_home\diff_out'  # 2026-08-25 启元改: 输出移D盘(354GB空闲), 原C盘 scratch_test/diff 27GB 爆盘
os.makedirs(OUT_DIR, exist_ok=True)

# ---- 半随机 C 程序生成器 ----

TYPES = ['int', 'unsigned', 'char', 'short', 'long', 'long long', 'double']
INT_TYPES = ['int', 'unsigned', 'char', 'short', 'long', 'long long']
VALUES = ['1', '2', '3', '7', '10', '-5', '100', '0xFF']
ARITH = ['+', '-', '*', '/', '%']
CMP = ['<', '>', '<=', '>=', '==', '!=']
LOGIC = ['&&', '||']
BIT = ['&', '|', '^', '<<', '>>']

def gen_expr(depth, vars_list):
    if depth <= 0 or random.random() < 0.3:
        # 叶子: 变量或常量
        if vars_list and random.random() < 0.6:
            return random.choice(vars_list)
        return random.choice(VALUES)
    kind = random.randint(0, 5)
    if kind == 0:
        return f'({gen_expr(depth-1, vars_list)} {random.choice(ARITH)} {gen_expr(depth-1, vars_list)})'
    if kind == 1:
        return f'({gen_expr(depth-1, vars_list)} {random.choice(CMP)} {gen_expr(depth-1, vars_list)})'
    if kind == 2:
        return f'({gen_expr(depth-1, vars_list)} {random.choice(BIT)} {gen_expr(depth-1, vars_list)})'
    if kind == 3:
        return f'({gen_expr(depth-1, vars_list)} {random.choice(LOGIC)} {gen_expr(depth-1, vars_list)})'
    if kind == 4:
        return f'({gen_expr(depth-1, vars_list)})'
    return f'-({gen_expr(depth-1, vars_list)})'

def gen_struct(rng):
    """随机 struct: 混合字段类型 (布局敏感测试)"""
    n = rng.randint(2, 5)
    fields = []
    for i in range(n):
        t = rng.choice(TYPES)
        fields.append(f'    {t} f{i};')
    name = f'S{random.randint(1, 999)}'
    return name, 'struct ' + name + ' {\n' + '\n'.join(fields) + '\n};'

def gen_program(seed):
    rng = random.Random(seed)
    lines = []
    lines.append('#include <stdio.h>')
    lines.append('#include <stdint.h>')
    
    # 随机 struct (布局敏感)
    structs = []
    if rng.random() < 0.5:
        for _ in range(rng.randint(1, 2)):
            name, decl = gen_struct(rng)
            structs.append(name)
            lines.append(decl)
    
    # 随机函数 (类型尺寸/运算)
    if rng.random() < 0.5:
        t = rng.choice(INT_TYPES)
        lines.append(f'{t} helper({t} a, {t} b) {{ return a {rng.choice(ARITH)} b; }}')
    
    lines.append('int main(void) {')
    
    # 局部变量 (类型尺寸)
    vars_list = []
    for i in range(rng.randint(2, 4)):
        t = rng.choice(TYPES)
        v = f'v{i}'
        init = rng.choice(VALUES)
        lines.append(f'    {t} {v} = {init};')
        vars_list.append(v)
    
    # 结构体变量
    if structs:
        sv = 'st'
        lines.append(f'    struct {structs[0]} {sv};')
        lines.append(f'    {sv}.f0 = {rng.choice(VALUES)};')
        vars_list.append(sv + '.f0')
    
    # 表达式求值 + printf (变参测试)
    for i in range(rng.randint(2, 4)):
        e = gen_expr(2, vars_list)
        fmt = rng.choice(['%d', '%u', '%ld', '%lld', '%x'])
        lines.append(f'    printf("{fmt}\\n", (long long)({e}));')
    
    # 控制流 (if/while)
    if rng.random() < 0.5:
        e = gen_expr(1, vars_list)
        lines.append(f'    if ({e}) printf("T\\n"); else printf("F\\n");')
    if rng.random() < 0.5:
        v = vars_list[0] if vars_list else 'v0'
        lines.append(f'    while ({v} > 0) {{ printf("W%ld\\n", (long long){v}); {v} = {v} - 1; }}')
    
    lines.append('    return 0;')
    lines.append('}')
    return '\n'.join(lines)

def run_and_capture(exe, cwd):
    try:
        p = subprocess.run([exe], capture_output=True, timeout=5, cwd=cwd)
        return p.stdout.decode('utf-8', 'replace').replace('\r\n', '\n'), p.returncode
    except subprocess.TimeoutExpired:
        return '<TIMEOUT>', -999

def compile_and_test(src_path, idx):
    """编译+运行+比对 qcc vs gcc"""
    name = os.path.splitext(os.path.basename(src_path))[0]
    q_exe = os.path.join(OUT_DIR, f'{name}_q.exe')
    g_exe = os.path.join(OUT_DIR, f'{name}_g.exe')
    
    # qcc 编译
    rq = subprocess.run([QCC, '-o', q_exe, src_path] + QCC_ARGS, capture_output=True, timeout=60)
    # gcc 编译
    rg = subprocess.run(['gcc', '-o', g_exe, src_path], capture_output=True, timeout=60)
    
    if rq.returncode != 0 and rg.returncode != 0:
        return 'skip'  # 两边都编译失败
    if rq.returncode != 0:
        return f'QCC_COMPILE_FAIL\n  qcc: {(rq.stdout+rq.stderr).decode("utf-8","replace")[-300:]}'
    if rg.returncode != 0:
        return 'gcc_fail_only'
    
    q_out, q_rc = run_and_capture(q_exe, OUT_DIR)
    g_out, g_rc = run_and_capture(g_exe, OUT_DIR)
    
    if q_out == g_out and q_rc == g_rc:
        return 'PASS'
    return f'DIFF\n  qcc: out={q_out[:200]!r} rc={q_rc}\n  gcc: out={g_out[:200]!r} rc={g_rc}'

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 50
    start_seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    results = {'PASS': 0, 'DIFF': [], 'skip': 0, 'gcc_fail_only': 0, 'compile_fail': 0}
    
    for i in range(n):
        seed = start_seed + i
        src = gen_program(seed)
        src_path = os.path.join(OUT_DIR, f't{seed}.c')
        with open(src_path, 'w', encoding='utf-8') as f:
            f.write(src)
        r = compile_and_test(src_path, seed)
        if r == 'PASS':
            results['PASS'] += 1
        elif r == 'skip':
            results['skip'] += 1
        elif r == 'gcc_fail_only':
            results['gcc_fail_only'] += 1
        else:
            results['DIFF'].append((seed, r))
    
    print(f'=== 差异测试: {n} 个程序 ===')
    print(f'PASS: {results["PASS"]}  skip(两边都失败): {results["skip"]}  gcc_fail_only: {results["gcc_fail_only"]}')
    print(f'差异: {len(results["DIFF"])} 个')
    for seed, r in results['DIFF'][:10]:
        print(f'--- t{seed} ---')
        print(r[:400])
        # 打印源程序
        src_path = os.path.join(OUT_DIR, f't{seed}.c')
        print('源程序:')
        print(open(src_path, encoding='utf-8').read()[:500])
    
    # 汇总
    if len(results['DIFF']) == 0:
        print('=== 全部 PASS (qcc vs gcc 输出一致) ===')
    else:
        print(f'=== 发现 {len(results["DIFF"])} 个差异 ===')

if __name__ == '__main__':
    main()
