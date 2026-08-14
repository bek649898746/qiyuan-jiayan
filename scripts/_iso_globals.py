# -*- coding: utf-8 -*-
"""进一步隔离: 函数声明在前是否破坏全局变量注册."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

cases = {
    'printf_decl_then_g_return': 'int printf(const char*, ...);\nint g = 10;\nint main() { return g; }',
    'foo_decl_then_g_printf':    'int foo(int);\nint g = 10;\nint printf(const char*, ...);\nint main() { printf("%d\\n", g); return 0; }',
    'printf_decl_2globals':      'int printf(const char*, ...);\nint g = 10;\nint h = 20;\nint main() { printf("%d %d\\n", g, h); return 0; }',
    'printf_decl_no_proto':      'int g = 10;\nint main() { printf("%d\\n", g); return 0; }',
    'printf_decl_then_static':   'int printf(const char*, ...);\nstatic int g = 10;\nint main() { printf("%d\\n", g); return 0; }',
    'printf_decl_then_chararr':  'int printf(const char*, ...);\nchar s[] = "hi";\nint main() { printf("%s\\n", s); return 0; }',
}
for tag, body in cases.items():
    open('scratch_test/_iso.c', 'w', encoding='utf-8').write(body + '\n')
    out = os.path.abspath('scratch_test/_iso_exe.exe')
    r = subprocess.run(['qcc_x86.exe', 'scratch_test/_iso.c', '-o', out], capture_output=True)
    if r.returncode != 0:
        print(f'{tag:28s} compile_rc={r.returncode}')
        continue
    p = subprocess.run(['cmd', '/c', out], capture_output=True, text=True, timeout=15)
    print(f'{tag:28s} rc={p.returncode} out={(p.stdout or "").strip()[:30]!r}')
