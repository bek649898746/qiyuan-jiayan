# -*- coding: utf-8 -*-
"""声明顺序对照."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
cases = {
    'printf_first': 'int printf(const char*, ...);\nint g = 10;\nint main() { printf("%d\\n", g); return 0; }',
    'g_first     ': 'int g = 10;\nint printf(const char*, ...);\nint main() { printf("%d\\n", g); return 0; }',
}
for tag, body in cases.items():
    open('scratch_test/_ord.c', 'w', encoding='utf-8').write(body + '\n')
    out = os.path.abspath('scratch_test/_ord_exe.exe')
    r = subprocess.run(['qcc_x86.exe', 'scratch_test/_ord.c', '-o', out], capture_output=True)
    if r.returncode != 0:
        print(tag, 'compile rc=', r.returncode)
        continue
    p = subprocess.run(['cmd', '/c', out], capture_output=True, text=True, timeout=15)
    print(tag, 'rc=', p.returncode, repr((p.stdout or '').strip()[:30]))
