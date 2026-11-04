# -*- coding: utf-8 -*-
"""check_elf_gate —— E 段 ELF64 后端 gating（WSL 存在时**必须真跑**）

断言：
  1. `cc --target=elf hello.c -o hello.elf` rc=0 且文件存在；
  2. `scripts/check_elf_format.py` 对产物 PASS（魔数/头/程序头/节头/段对齐/入口/非空壳）；
  3. **WSL 真跑**：`./hello.elf` 输出 `hello`、rc=0（WSL 不可用 ⟹ 判红，不静默降级）；
  4. 同一份源码走 PE 路径仍可编译、可运行，输出 `hello`、rc=0（PE 零回归的行为面）。

可选 `--cc <exe>` 指定编译器（默认 qcc_zh_v2.exe）；`--no-run` 只做 1/2/4。
退出码: 0 = 全绿；1 = 失败；2 = 环境未就绪。
"""
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
sys.path.insert(0, os.path.join(ROOT, 'scripts'))

try:
    from qcc_tmp import tmp_dir
except Exception as e:
    print('[FAIL] 缺 scripts/qcc_tmp.py（测试仓位纪律）: %s' % e)
    sys.exit(2)

CC = 'qcc_zh_v2.exe'
if '--cc' in sys.argv:
    CC = sys.argv[sys.argv.index('--cc') + 1]
NO_RUN = '--no-run' in sys.argv
if not os.path.exists(CC):
    print('[FAIL] 编译器不存在: %s' % CC)
    sys.exit(2)

work = tmp_dir('qcc_elf_gate')
src = os.path.join(work, 'hello.c')
with open(src, 'w', encoding='utf-8', newline='\n') as f:
    f.write('#include <stdio.h>\nint main(void) { printf("hello\\n"); return 0; }\n')

fails = []


def run(a, t=600):
    return subprocess.run(a, capture_output=True, timeout=t)


def wsl_path(p):
    p = os.path.abspath(p)
    drive = p[0].lower()
    return '/mnt/' + drive + p[2:].replace('\\', '/')


def wsl_ready():
    try:
        r = subprocess.run(['wsl', '-d', 'Ubuntu', '--', 'bash', '-c', 'true'],
                           capture_output=True, timeout=90)
        return r.returncode == 0
    except Exception:
        return False


elf = os.path.join(work, 'hello.elf')
r = run([CC, '--target=elf', src, '-o', elf])
print('  ① ELF 产物: rc=%d  %s' % (r.returncode, elf))
if r.returncode != 0 or not os.path.exists(elf):
    fails.append('--target=elf 产物生成失败')
else:
    r2 = run([sys.executable, os.path.join('scripts', 'check_elf_format.py'), elf])
    out = (r2.stdout + r2.stderr).decode('utf-8', 'replace')
    print('  ② 格式判据: rc=%d' % r2.returncode)
    for line in out.strip().splitlines()[-7:]:
        print('     ' + line)
    if r2.returncode != 0:
        fails.append('ELF 格式判据 FAIL')
    if not NO_RUN:
        if not wsl_ready():
            print('  ③ WSL: 不可用（`wsl -d Ubuntu` 失败）')
            fails.append('WSL 不可用 ⟹ ELF 运行档无法验证（不得静默降级）')
        else:
            wp = wsl_path(elf)
            cmd = 'chmod +x "%s" && "%s"' % (wp, wp)
            r3 = run(['wsl', '-d', 'Ubuntu', '--', 'bash', '-c', cmd], 300)
            out3 = (r3.stdout or b'').decode('utf-8', 'replace').strip()
            err3 = (r3.stderr or b'').decode('utf-8', 'replace').strip()
            print('  ③ WSL 运行: rc=%d out=%r' % (r3.returncode, out3))
            if err3:
                print('     stderr: %s' % err3[-200:])
            if r3.returncode != 0 or out3 != 'hello':
                fails.append('ELF 在 WSL 运行结果不符（rc=%d out=%r）' % (r3.returncode, out3))

exe = os.path.join(work, 'hello.exe')
r = run([CC, src, '-o', exe])
print('  ④ PE 路径编译: rc=%d' % r.returncode)
if r.returncode != 0 or not os.path.exists(exe):
    fails.append('PE 路径编译失败')
else:
    r = run([exe], 120)
    out = (r.stdout or b'').decode('utf-8', 'replace').strip()
    print('  ⑤ PE 运行: rc=%d out=%r' % (r.returncode, out))
    if r.returncode != 0 or out != 'hello':
        fails.append('PE 运行结果不符')

if fails:
    print('== ELF 门: FAIL -> %s ==' % '; '.join(fails))
    sys.exit(1)
print('== ELF 门: PASS（ELF64 格式自洽 + WSL 真跑 hello + PE 路径零回归）==')
sys.exit(0)
