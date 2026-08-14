# -*- coding: utf-8 -*-
"""对 b_global 逐行删减, 找 with_rt 时的失败触发点."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

BASE = '''int printf(const char*, ...);
int g = 10;
long long gl = 3000000000LL;
double gd = 3.14;
int main() {
    printf("%d\\n", g);
    printf("%lld\\n", gl);
    printf("%.2f\\n", gd);
    g += 5;
    gl *= 2;
    printf("%d %lld\\n", g, gl);
    return 0;
}'''

variants = {
    'full': BASE,
    'no_gd_decl': BASE.replace('double gd = 3.14;\n', ''),
    'no_gd_use': BASE.replace('    printf("%.2f\\n", gd);\n', ''),
    'no_gl': BASE.replace('long long gl = 3000000000LL;\n', '').replace('gl', 'g').replace('"%.2f\\n", g)', '"%.2f\\n", g)'),
    'no_ll_printf': BASE.replace('    printf("%lld\\n", gl);\n', ''),
    'no_assign': BASE.replace('    g += 5;\n', '').replace('    gl *= 2;\n', ''),
    'no_last': BASE.replace('    printf("%d %lld\\n", g, gl);\n', ''),
    'only_first': '''int printf(const char*, ...);
int g = 10;
int main() { printf("%d\\n", g); return 0; }''',
    'first_plus_dbl': '''int printf(const char*, ...);
int g = 10;
double gd = 3.14;
int main() { printf("%d\\n", g); printf("%.2f\\n", gd); return 0; }''',
    'dbl_then_int': '''int printf(const char*, ...);
int g = 10;
double gd = 3.14;
int main() { printf("%.2f\\n", gd); printf("%d\\n", g); return 0; }''',
}

exp = '10\n3000000000\n3.14\n15 1705032704'
for tag, src in variants.items():
    open('scratch_test/_bis.c', 'w', encoding='utf-8').write(src + '\n')
    out = os.path.abspath('scratch_test/_bis_exe.exe')
    r = subprocess.run(['qcc_x86.exe', 'scratch_test/_bis.c', '-o', out], capture_output=True)
    if r.returncode != 0:
        print(f'{tag:18s} compile_rc={r.returncode}')
        continue
    p = subprocess.run(['cmd', '/c', out], capture_output=True, text=True, timeout=15)
    o = (p.stdout or '').strip()
    print(f'{tag:18s} rc={p.returncode} ok={(o==exp)} out={o[:50]!r}')
