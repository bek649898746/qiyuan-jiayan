# -*- coding: utf-8 -*-
"""最小化 b_global 的 printf 参数 bug: 各种参数形式 + 有无 qcc_rt."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

tests = {
    'plain_const':   'int printf(const char*, ...); int main() { printf("%d\\n", 10); return 0; }',
    'plain_var':     'int g = 10; int printf(const char*, ...); int main() { printf("%d\\n", g); return 0; }',
    'local_var':     'int printf(const char*, ...); int main() { int x = 10; printf("%d\\n", x); return 0; }',
    'static_local':  'int printf(const char*, ...); int main() { static int x = 10; printf("%d\\n", x); return 0; }',
    'global_direct': 'int g = 10; int printf(const char*, ...); int main() { return g; }',
    'two_args':      'int g = 10; int printf(const char*, ...); int main() { printf("%d %d\\n", g, g); return 0; }',
    'global_ll':     'long long gl = 3000000000LL; int printf(const char*, ...); int main() { printf("%lld\\n", gl); return 0; }',
}
for tag, src in tests.items():
    open('scratch_test/_min.c', 'w', encoding='utf-8').write(src + '\n')
    results = []
    for cwd, label in [(r'C:\Users\Administrator\Desktop\qiyuan-jiayan', 'with_rt '), ('scratch_test', 'no_rt   ')]:
        out = os.path.join('scratch_test', f'_min_{tag}.exe')
        r = subprocess.run(['qcc_x86.exe', 'scratch_test/_min.c', '-o', out], cwd=cwd, capture_output=True)
        if r.returncode != 0:
            results.append(f'{label}:compile_rc={r.returncode}')
            continue
        p = subprocess.run(['cmd', '/c', os.path.abspath(out)], capture_output=True, text=True, timeout=15)
        results.append(f'{label}:rc={p.returncode} out={(p.stdout or "").strip()!r}')
    print(f'{tag:15s} | ' + ' | '.join(results))
