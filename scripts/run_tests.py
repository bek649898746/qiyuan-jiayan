# -*- coding: utf-8 -*-
"""甲言 行为断言测试运行器 (Python 版)
用法: python scripts/run_tests.py
断言格式（测试 .c 头部注释）:
  // @EXPECTED exit:0           退出码 == 0
  // @EXPECTED exit:42          退出码 == 42
  // @EXPECTED exit:nonzero     退出码 != 0 (探针/崩溃)
  // @EXPECTED out:<str>        stdout 包含该字符串 (可选)
无 @EXPECTED 时默认 exit:0
"""
import sys, os, subprocess, re
sys.stdout.reconfigure(encoding='utf-8')

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(root)
os.makedirs('scratch_test', exist_ok=True)

# ===== [DIAG-0817] 诊断段: Server 2025 0xC0000005 根因探测 (PR 专用, 不会合入 main) =====
import struct
def _crash_events(tag):
    """读 Windows 事件日志中的崩溃记录 (Application Error 1000)"""
    try:
        ps = ('powershell', '-NoProfile', '-Command',
              'Get-WinEvent -FilterHashtable @{LogName="Application"; Id=1000} -MaxEvents 3 '
              '-ErrorAction SilentlyContinue | Select-Object -First 3 TimeCreated, '
              '@{n="Msg";e={$_.Message.Substring(0, [Math]::Min(500, $_.Message.Length))}} | '
              'Format-List | Out-String')
        r = subprocess.run(ps, capture_output=True, text=True, errors='replace', timeout=20)
        txt = (r.stdout or '').strip()
        print('[DIAG][%s] crash events:\n%s' % (tag, txt[:1200] if txt else '(none)'))
    except Exception as e:
        print('[DIAG][%s] event-read EXC %r' % (tag, e))

def _pe_diag(exe):
    try:
        d = open(exe, 'rb').read()
        pe = struct.unpack_from('<I', d, 0x3C)[0]
        opt = pe + 24
        sizeimage = struct.unpack_from('<I', d, opt + 56)[0]
        entry = struct.unpack_from('<I', d, opt + 16)[0]
        imp_rva, imp_sz = struct.unpack_from('<II', d, opt + 120)
        nsects = struct.unpack_from('<H', d, pe + 6)[0]
        sec = opt + 112
        out = ['[DIAG] %s: sizeimage=0x%X entry=0x%X import=0x%X/%d' % (exe, sizeimage, entry, imp_rva, imp_sz)]
        for i in range(nsects):
            off = sec + i * 40
            nm = d[off:off + 8].rstrip(b'\0').decode(errors='replace')
            vsz, va, rsz, roff = struct.unpack_from('<IIII', d, off + 8)
            out.append('[DIAG]   %s: va=0x%X vsz=0x%X raw=0x%X/0x%X' % (nm, va, vsz, roff, rsz))
            if nm == '.data':
                stk = 0x10405D00
                out.append('[DIAG]   .data raw_end=0x%X vs_end=0x%X stk=0x%X d_raw=0x%X d_vs=0x%X'
                           % (va + rsz, va + vsz, stk, stk - (va + rsz), stk - (va + vsz)))
        return '\n'.join(out)
    except Exception as e:
        return '[DIAG] %s: ERR %r' % (exe, e)

def _run_diag():
    q = os.path.join(root, 'qcc_x86.exe')
    if not os.path.exists(q):
        print('[DIAG] qcc_x86.exe 不存在, 跳过'); return
    try:
        gv = subprocess.run(['gcc', '--version'], capture_output=True, text=True, errors='replace', timeout=15)
        print('[DIAG] gcc: %s' % (gv.stdout or gv.stderr or '').splitlines()[0])
    except Exception as e:
        print('[DIAG] gcc-version EXC %r' % e)
    for src, name in [('tests/qcc/ret_a.c', 'ret_a'), ('tests/qcc/simple_call.c', 'simple_call'),
                      ('tests/behavior/b_scanf.c', 'b_scanf')]:
        exe = os.path.join('scratch_test', name + '_diag.exe')
        try: os.remove(exe)
        except OSError: pass
        r = subprocess.run([q, src, '-o', exe], capture_output=True, text=True, errors='replace')
        if r.returncode != 0 or not os.path.exists(exe):
            print('[DIAG] %s: compile FAIL rc=%s' % (name, r.returncode)); continue
        print(_pe_diag(exe))
        # 1) cmd /c 无重定向(继承终端)
        try:
            p1 = subprocess.run(['cmd', '/c', exe], timeout=10)
            print('[DIAG] %s [cmd-inherit] rc=%s' % (name, p1.returncode))
        except Exception as e:
            print('[DIAG] %s [cmd-inherit] EXC %r' % (name, e))
        # 2) cmd /c + capture_output (PIPE) — 完全复刻 run_tests.py 原调用
        try:
            p2 = subprocess.run(['cmd', '/c', exe], capture_output=True, timeout=10)
            print('[DIAG] %s [cmd-pipe] rc=%s out=%r' % (name, p2.returncode, p2.stdout[:40]))
        except Exception as e:
            print('[DIAG] %s [cmd-pipe] EXC %r' % (name, e))
        # 3) cmd /c + 文件重定向 (> out.txt)
        of = os.path.join('scratch_test', name + '_diag_out.txt')
        try:
            p3 = subprocess.run(['cmd', '/c', '%s > %s 2>&1' % (exe, of)], timeout=10)
            txt = open(of, 'rb').read(40) if os.path.exists(of) else b''
            print('[DIAG] %s [cmd-file] rc=%s out=%r' % (name, p3.returncode, txt))
        except Exception as e:
            print('[DIAG] %s [cmd-file] EXC %r' % (name, e))
        # 4) subprocess 直启 (8-08 报告 Server 2025 必 0xC0000005)
        try:
            p4 = subprocess.run([exe], capture_output=True, timeout=10)
            print('[DIAG] %s [direct-pipe] rc=%s out=%r' % (name, p4.returncode, p4.stdout[:40]))
        except Exception as e:
            print('[DIAG] %s [direct-pipe] EXC %r' % (name, e))
        # 5) Popen 直启
        try:
            po = subprocess.Popen([exe], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                o, _ = po.communicate(timeout=10); print('[DIAG] %s [popen] rc=%s out=%r' % (name, po.returncode, o[:40]))
            except Exception:
                po.kill(); print('[DIAG] %s [popen] KILLED' % name)
        except Exception as e:
            print('[DIAG] %s [popen] EXC %r' % (name, e))
        _crash_events(name)
    # 终极判别: dump 运行时 IAT 值 (动态解析自己 PE 头 — data_rva_base 随代码大小变化, 不能硬编码地址)
    for src, name in [('_iat_dump.c', 'iatdump')]:
        sp = os.path.join('scratch_test', src)
        with open(sp, 'w') as f:
            f.write('#include <stdio.h>\n'
                    'int main(void){\n'
                    '    unsigned char *p = (unsigned char*)0x400000;\n'
                    '    unsigned int pe = *(unsigned int*)(p + 0x3C);\n'
                    '    unsigned int imp_rva = *(unsigned int*)(p + pe + 24 + 120);\n'
                    '    printf("impRVA=%x\\n", imp_rva);\n'
                    '    unsigned char *d = p + imp_rva;\n'
                    '    unsigned int ft = *(unsigned int*)(d + 16);\n'
                    '    printf("firstThunk=%x\\n", ft);\n'
                    '    unsigned long long *iat = (unsigned long long*)(p + ft);\n'
                    '    for (int i = 0; i < 10; i++) { if (iat[i] == 0) break; printf("IAT[%d]=%llx\\n", i, iat[i]); }\n'
                    '    return 0;\n'
                    '}\n')
        exe = os.path.join('scratch_test', name + '_diag.exe')
        try: os.remove(exe)
        except OSError: pass
        r = subprocess.run([q, sp, '-o', exe], capture_output=True, text=True, errors='replace')
        if r.returncode != 0 or not os.path.exists(exe):
            print('[DIAG] %s: compile FAIL rc=%s' % (name, r.returncode)); continue
        print(_pe_diag(exe))
        try:
            p1 = subprocess.run(['cmd', '/c', exe], capture_output=True, timeout=4)
            print('[DIAG] %s [cmd-pipe] rc=%s out=%r' % (name, p1.returncode, (p1.stdout or b'')[:300]))
        except subprocess.TimeoutExpired:
            print('[DIAG] %s TIMEOUT(死循环正常)' % name)
        except Exception as e:
            print('[DIAG] %s EXC %r' % (name, e))
        _crash_events(name)
    # 决定性对照: gcc 编译标准程序 (标准 CRT 栈) 在 CI 上是否正常退出 — 排除"环境拦截所有退出"
    import shutil as _sh
    gcc_src = os.path.join('scratch_test', '_gcc_hello.c')
    with open(gcc_src, 'w') as f:
        f.write('#include <stdio.h>\nint main(void){ printf("gcc-hello\\n"); return 0; }\n')
    gcc_exe = os.path.join('scratch_test', 'gcc_hello.exe')
    try: os.remove(gcc_exe)
    except OSError: pass
    rg = subprocess.run(['gcc', '-O2', gcc_src, '-o', gcc_exe], capture_output=True, text=True, errors='replace')
    if rg.returncode == 0 and os.path.exists(gcc_exe):
        try:
            p1 = subprocess.run(['cmd', '/c', gcc_exe], capture_output=True, timeout=10)
            print('[DIAG] gcc-hello [cmd-pipe] rc=%s out=%r' % (p1.returncode, (p1.stdout or b'')[:40]))
        except Exception as e:
            print('[DIAG] gcc-hello EXC %r' % e)
        try:
            p2 = subprocess.run([gcc_exe], capture_output=True, timeout=10)
            print('[DIAG] gcc-hello [direct] rc=%s' % p2.returncode)
        except Exception as e:
            print('[DIAG] gcc-hello [direct] EXC %r' % e)
    else:
        print('[DIAG] gcc-hello compile FAIL rc=%s' % rg.returncode)
    # 系统 CET / 安全特性状态
    try:
        ps = ('powershell', '-NoProfile', '-Command',
              'Get-ProcessMitigation -System -ErrorAction SilentlyContinue | Format-List * | Out-String')
        r = subprocess.run(ps, capture_output=True, text=True, errors='replace', timeout=30)
        print('[DIAG] system mitigations:\n%s' % (r.stdout or '')[:1500])
    except Exception as e:
        print('[DIAG] mitigation EXC %r' % e)
    # gdb 崩溃现场: 直接看退出路径崩在哪 (CI 的 Git 自带 MinGW gdb)
    gdb_paths = [r'C:\Program Files\Git\mingw64\bin\gdb.exe', r'C:\Program Files\Git\usr\bin\gdb.exe']
    gdb = next((p for p in gdb_paths if os.path.exists(p)), None)
    if not gdb:
        import shutil
        gdb = shutil.which('gdb')
    if gdb:
        print('[DIAG] gdb at %s' % gdb)
        for exe in ['ret_a', 'iatdump']:
            exep = os.path.join('scratch_test', exe + '_diag.exe')
            if not os.path.exists(exep): continue
            try:
                r = subprocess.run([gdb, '-batch', '-ex', 'run', '-ex', 'bt', '--args', exep],
                                   capture_output=True, text=True, errors='replace', timeout=40)
                out = (r.stdout or '') + (r.stderr or '')
                print('[DIAG][gdb-%s] rc=%s\n%s' % (exe, r.returncode, out[:1500]))
            except Exception as e:
                print('[DIAG][gdb-%s] EXC %r' % (exe, e))
    else:
        print('[DIAG] gdb NOT FOUND')
_run_diag()
# ===== [DIAG-0817] 诊断段结束 =====

# 1. 确保编译器
qcc = os.path.join(root, 'qcc_x86.exe')
if not os.path.exists(qcc):
    print('[1] 编译宿主 qcc_x86 ...')
    r = subprocess.run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/qcc_x86.c', '-o', qcc])
    if r.returncode != 0:
        print('[FAIL] 编译宿主失败'); sys.exit(1)

# 清理 scratch_test: CI 多次 run 残留文件会被占用 -> cannot write _H1.exe (fix 2026-08-08)
import shutil
st_dir = os.path.join(root, 'scratch_test')
# 先杀僵尸测试进程 (崩溃测试的 exe 进程可能残留锁住 _H1.exe, 复跑必失败 fix 2026-08-09;
# taskkill 通配符不可靠, 用 PowerShell Get-Process + Stop-Process 实测有效)
subprocess.run(['powershell', '-NoProfile', '-Command',
                'Get-Process -Name "*_H1*" -ErrorAction SilentlyContinue | Stop-Process -Force'],
               capture_output=True)
if os.path.isdir(st_dir):
    for f in os.listdir(st_dir):
        try:
            os.remove(os.path.join(st_dir, f))
        except OSError:
            pass
else:
    os.makedirs(st_dir, exist_ok=True)

tests = sorted('qcc/' + f for f in os.listdir('tests/qcc') if f.endswith('.c'))
tests += sorted('behavior/' + f for f in os.listdir('tests/behavior') if f.endswith('.c'))
pass_n = fail_n = 0
fails = []

for name in tests:
    src = os.path.join('tests', name)  # name 已含 qcc/ 或 behavior/ 前缀
    # 读断言
    expected = '0'; expected_out = None; expect_cfail = False; stdin_in = None
    try:
        head = open(src, encoding='utf-8', errors='replace').read(600)
    except Exception:
        head = ''
    for line in head.split('\n'):
        m = re.search(r'//\s*@EXPECTED\s+exit:\s*(\S+)', line)
        if m: expected = m.group(1)
        m = re.search(r'//\s*@EXPECTED\s+out:(.+)$', line)
        if m: expected_out = m.group(1).strip()
        m = re.search(r'//\s*@EXPECTED\s+in:(.+)$', line)
        if m: stdin_in = m.group(1).strip()
        if '@EXPECTED compile_fail' in line: expect_cfail = True

    # H1 编译 (输出名用 basename, 兼容 behavior/ 子目录)
    h1 = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_H1.exe')
    try:
        os.remove(h1)
    except OSError:
        pass
    r = subprocess.run([qcc, src, '-o', h1], capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0 or not os.path.exists(h1):
        if expect_cfail:
            pass_n += 1; continue  # @EXPECTED compile_fail: 编译失败 = 通过 (fix 2026-08-12)
        fail_n += 1; fails.append(f'{name} 编译失败'); continue

    # 运行
    out_file = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_out.txt')
    err_file = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_err.txt')
    try:
        os.remove(out_file); os.remove(err_file)
    except OSError:
        pass
    try:
        # 用 cmd /c 运行: Server 2025 上 Python 直接 CreateProcess qcc 生成 PE 会 0xC0000005,
        # 但 cmd 启动正常 (fix 2026-08-08, 实测 CI 诊断: cmd rc=0, subprocess rc=0xC0000005)
        p = subprocess.run(['cmd', '/c', h1], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=15, input=stdin_in)
        rc = p.returncode
        out = (p.stdout or '').strip()
    except subprocess.TimeoutExpired as te:
        # 超时后必须 kill: 崩溃/卡死进程残留会锁住 _H1.exe, 下次覆盖写失败 (fix 2026-08-08)
        try:
            te.kill()  # 杀整个进程组 (包含可能的子进程)
        except Exception:
            pass
        rc = 'TIMEOUT'; out = ''
    except Exception as e:
        rc = 'ERR'; out = str(e)
    try:
        os.remove(h1); os.remove(out_file); os.remove(err_file)
    except OSError:
        pass

    # 断言
    ok = False
    if expected == 'nonzero':
        ok = (rc != 0)
    else:
        ok = (str(rc) == expected)
    if ok and expected_out:
        ok = expected_out in out

    if ok:
        pass_n += 1
    else:
        fail_n += 1
        fails.append(f"{name} (期望exit={expected} 实际={rc} out='{out[:40]}')")

print(f'\n行为断言: PASS={pass_n} FAIL={fail_n}')
if fails:
    print('--- 失败 ---')
    for f in fails:
        print(f'  {f}')
    sys.exit(1)
print('==== 全绿 ====')
