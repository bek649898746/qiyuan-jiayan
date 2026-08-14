# -*- coding: utf-8 -*-
"""Git 逐文件试编译: 用 qcc 编译 Git 核心 .c, 统计通过/失败分类."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
QCC = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\qcc_x86.exe'
OUT = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\scratch_test\gitc'
os.makedirs(OUT, exist_ok=True)

D = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1']

# Git 核心文件 (按依赖从小到大)
files = ['hex.c', 'strbuf.c', 'hash-lookup.c', 'oid-array.c', 'strvec.c', 'date.c', 'dir.c',
         'path.c', 'environment.c', 'strmap.c', 'string-list.c', 'trace2.c',
         'pretty.c', 'revision.c', 'commit.c', 'object.c', 'cache-tree.c']

ok = []
fail = []
for f in files:
    p = os.path.join(GIT, f)
    if not os.path.exists(p):
        continue
    out = os.path.join(OUT, f.replace('.c', '.exe'))
    r = subprocess.run([QCC] + D + [f, '-o', out], capture_output=True, timeout=120, cwd=GIT)
    err = (r.stdout + r.stderr).decode('utf-8', 'replace')
    if r.returncode == 0:
        ok.append(f)
        print('OK   %-30s' % f)
    else:
        # 提取错误首行
        for line in err.splitlines():
            if '[ERR]' in line or 'error' in line.lower():
                fail.append((f, line.strip()[:100]))
                print('FAIL %-30s | %s' % (f, line.strip()[:90]))
                break
        else:
            fail.append((f, err.strip()[:90]))
            print('FAIL %-30s | %s' % (f, err.strip()[:90]))

print('=' * 60)
print('通过 %d / 共 %d' % (len(ok), len(ok) + len(fail)))
for f, e in fail:
    print(' ', f, ':', e)
