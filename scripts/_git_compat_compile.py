# -*- coding: utf-8 -*-
"""Git compat/ 平台层编译基线: v4 逐个编译, 统计通过/失败/崩溃."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
V4 = os.environ.get('QCC', r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v4.exe')
D = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1', '-D', 'NO_MBSUPPORT',
     '-D', 'CHAR_BIT=8', '-D', 'UINT_MAX=4294967295', '-D', 'INT_MAX=2147483647', '-D', 'ULONG_MAX=4294967295', '-D', 'LONG_MAX=2147483647']
# 非独立编译单元 (被其他 .c 以 #include 内联, 不单独编译) + 平台特定跳过
SKIP = {
    'compat\\regex\\regex_internal.c', 'compat\\regex\\regcomp.c', 'compat\\regex\\regexec.c',  # regex.c 内联
    'compat\\mingw.c',                                                                          # msvc.c 内联
    'compat\\win32\\headless.c',                                                               # L 宽字符
    'compat\\simple-ipc\\ipc-win32.c',                                                        # accctrl.h Windows SDK
    'compat\\simple-ipc\\ipc-shared.c', 'compat\\simple-ipc\\ipc-unix-socket.c',              # 需 SUPPORTS_SIMPLE_IPC 平台宏 + Unix socket 系统头
}

files = []
for root, dirs, names in os.walk(os.path.join(GIT, 'compat')):
    for f in sorted(names):
        if f.endswith('.c'):
            rel = os.path.relpath(os.path.join(root, f), GIT)
            if rel in SKIP:
                continue
            files.append(rel)

ok, fail, crash, timeout = [], [], [], []
for rel in files:
    p = os.path.join(GIT, rel)
    out = os.path.join(GIT, '_c_' + os.path.basename(rel)[:-2] + '.exe')
    args = [V4] + D
    # 预置头: 补 <errno.h> 系统头被跳过后的 errno + E* 常量
    args += ['-I', os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'compat_prelude.h')]
    if rel == 'compat\\regex\\regex.c':  # regex.c 的 <regex.h> 是系统头(qcc 跳过), 手动预置
        args += ['-I', os.path.join(GIT, 'compat', 'regex', 'regex.h')]
    args += [p, '-o', out]
    try:
        r = subprocess.run(args, capture_output=True, timeout=60, cwd=GIT)
        err = (r.stdout + r.stderr).decode('utf-8', 'replace')
        if r.returncode == 0:
            ok.append(rel)
        elif r.returncode < 0:
            crash.append((rel, r.returncode))
        else:
            first = next((l.strip()[:90] for l in err.splitlines() if '[ERR]' in l or 'error' in l.lower()), err.strip()[:90])
            fail.append((rel, first))
    except subprocess.TimeoutExpired:
        timeout.append(rel)
    finally:
        try: os.remove(out)
        except OSError: pass

print('=' * 70)
print('v4 compat/ 编译基线 (%d 个 .c):' % len(files))
print('  OK      : %d' % len(ok))
print('  FAIL    : %d' % len(fail))
print('  CRASH   : %d' % len(crash))
print('  TIMEOUT : %d' % len(timeout))
print('=' * 70)
for f, e in fail:
    print('  FAIL %-40s | %s' % (f, e))
for f, rc in crash:
    print('  CRASH %-40s rc=0x%08X' % (f, rc & 0xFFFFFFFF))
for f in timeout:
    print('  TIMEOUT %s' % f)
