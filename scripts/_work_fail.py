# -*- coding: utf-8 -*-
"""编译工作版/失败版对照二进制."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
work = 'int g = 10;\nint printf(const char*, ...);\nint main() { printf("%d\\n", g); return 0; }'
fail = 'int printf(const char*, ...);\nint g = 10;\nint main() { printf("%d\\n", g); return 0; }'
open('scratch_test/_work.c', 'w', encoding='utf-8').write(work + '\n')
open('scratch_test/_fail.c', 'w', encoding='utf-8').write(fail + '\n')
for fn in ['_work', '_fail']:
    out = os.path.abspath('scratch_test/%s.exe' % fn)
    r = subprocess.run(['qcc_x86.exe', 'scratch_test/%s.c' % fn, '-o', out], capture_output=True)
    print(fn, 'compile', r.returncode)
    if r.returncode == 0:
        p = subprocess.run(['cmd', '/c', out], capture_output=True, text=True, timeout=15)
        print('   run rc=', p.returncode, repr((p.stdout or '').strip()[:30]))
