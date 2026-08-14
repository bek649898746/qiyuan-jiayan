# -*- coding: utf-8 -*-
"""复现套件内 b_global 的异常: 编译到 scratch_test/b_global_H1.exe, cmd /c 相对路径运行."""
import subprocess, os, sys, io, shutil
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
qcc = 'qcc_x86.exe'

# 清理 _H1 进程 (套件也这么做)
subprocess.run(['powershell', '-NoProfile', '-Command',
                'Get-Process -Name "*_H1*" -ErrorAction SilentlyContinue | Stop-Process -Force'],
               capture_output=True)

bad = 0
for i in range(25):
    h1 = os.path.join('scratch_test', 'b_global_H1.exe')
    r = subprocess.run([qcc, 'tests/behavior/b_global.c', '-o', h1], capture_output=True)
    if r.returncode != 0:
        print('compile fail', i, r.returncode); continue
    p = subprocess.run(['cmd', '/c', h1], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=15)
    out = (p.stdout or '').strip()
    if out != '10\n3000000000\n3.14\n15 1705032704':
        bad += 1
        print('BAD run', i, 'rc=', p.returncode, repr(out))
print('done, bad =', bad)
