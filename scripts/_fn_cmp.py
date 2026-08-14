# -*- coding: utf-8 -*-
"""同内容不同文件名对照: 源码文件名是否影响编译结果."""
import subprocess, os, hashlib, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

src_body = 'int printf(const char*, ...);\nint g = 10;\nint main() { printf("%d\\n", g); return 0; }\n'
for fn in ['_min.c', '_bis.c', '_aaa.c', 't1.c']:
    p = os.path.join('scratch_test', fn)
    open(p, 'w', encoding='utf-8').write(src_body)
    out = os.path.abspath('scratch_test/_cmp_exe.exe')
    r = subprocess.run(['qcc_x86.exe', p, '-o', out], capture_output=True)
    if r.returncode != 0:
        print(f'{p:22s} compile rc={r.returncode}')
        continue
    d = open(out, 'rb').read()
    run = subprocess.run(['cmd', '/c', out], capture_output=True, text=True, timeout=15)
    print(f'{p:22s} sha={hashlib.sha256(d).hexdigest()[:12]} size={len(d)} rc={run.returncode} out={run.stdout.strip()!r}')
