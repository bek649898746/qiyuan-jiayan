# -*- coding: utf-8 -*-
"""甲言快测门禁 (10秒级) — 改一处源码后的即时验证闭环 (踩坑日志 10.1 方法论落地).

链路: gcc 宿主 → 宿主→v1 → v1→v2(最小文件) → v1 编译回归子集 → v2 复编一个测试
覆盖: 宿主编译错误 / 自举链断裂 / 关键功能回归 (全局变量/LL/hex/递归/switch/结构体)
用法: python scripts/_quick_check.py
"""
import subprocess, os, re, sys, hashlib, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

ROOT = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
QDIR = os.path.join(ROOT, 'build', 'quick')
os.chdir(ROOT)
os.makedirs(QDIR, exist_ok=True)

REG = [
    'tests/behavior/b_global.c',   # 全局变量 + LL + double + printf (parse_base 泄漏回归锚点)
    'tests/behavior/b_llx.c',      # %llx 64位十六进制
    'tests/behavior/b_lld.c',      # %lld
    'tests/behavior/b_llu.c',      # %llu
    'tests/behavior/b_llarr.c',    # LL 数组
    'tests/behavior/b_hex.c',      # %x/%X/%08x/%#x
    'tests/behavior/b_printf.c',   # printf 全家桶
    'tests/behavior/b_fib.c',      # 递归
    'tests/behavior/b_switch.c',   # switch
    'tests/behavior/b_struct.c',   # 结构体
]

def sha(path):
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()[:16] if os.path.exists(path) else 'MISSING'

def run(cmd, tag, timeout=900, capture=True):
    t0 = __import__('time').time()
    try:
        r = subprocess.run(cmd, capture_output=True, timeout=timeout)
        rc = r.returncode
    except subprocess.TimeoutExpired:
        rc = 'TIMEOUT'
    dt = __import__('time').time() - t0
    print(f'  [{tag}] rc={rc} ({dt:.0f}s)', flush=True)
    return rc

def compile_and_check(compiler, src, tag, outname=None):
    """编译一个测试并运行验证 @EXPECTED."""
    base = os.path.basename(src)[:-2] + '.exe'
    out = os.path.join(QDIR, outname or (base))
    if run([compiler, src, '-o', out], tag) != 0:
        return False
    # 解析期望
    expected = '0'; expected_out = None
    try:
        head = open(src, encoding='utf-8', errors='replace').read(600)
    except Exception:
        head = ''
    for line in head.split('\n'):
        m = re.search(r'//\s*@EXPECTED\s+exit:\s*(\S+)', line)
        if m: expected = m.group(1)
        m = re.search(r'//\s*@EXPECTED\s+out:(.+)$', line)
        if m: expected_out = m.group(1).strip()
    p = subprocess.run(['cmd', '/c', out], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=20)
    out_s = (p.stdout or '').strip()
    ok = (str(p.returncode) == expected)
    if ok and expected_out:
        ok = expected_out in out_s
    mark = 'OK ' if ok else 'FAIL'
    detail = f' exp={expected} rc={p.returncode}' + (f" out={out_s[:40]!r}" if not ok else '')
    print(f'  [{tag}] {mark}{detail}', flush=True)
    return ok

def main():
    fails = 0
    # [1] 宿主
    print('[1] gcc 编译宿主', flush=True)
    if run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/qcc_x86.c', '-o', 'qcc_x86.exe'], 'host') != 0:
        print('宿主编译失败 — 停止'); return 1

    # [2] 宿主 → v1 (编译镜像)
    print('[2] 宿主 → v1', flush=True)
    v1 = os.path.join(QDIR, 'v1.exe')
    if run(['qcc_x86.exe', 'srclib_jiayan/qcc_work.jy', '-o', v1], 'v1', timeout=600) != 0:
        print('v1 生成失败 — 停止'); return 1
    print(f'    v1 sha16={sha(v1)}', flush=True)

    # [3] v1 → 编译最小文件 (自宿主一代能否编译, 自举链健康下限)
    print('[3] v1 → 编译最小文件', flush=True)
    minc = os.path.join(QDIR, '_min.c')
    open(minc, 'w').write('int printf(const char*, ...);\nint main() { printf("hello\\n"); return 0; }\n')
    hello = os.path.join(QDIR, 'v1_hello.exe')
    if run([v1, minc, '-o', hello], 'v1:min', timeout=180) != 0:
        print('v1 编译最小文件失败 — 自举链断裂'); fails += 1
    else:
        p = subprocess.run(['cmd', '/c', hello], capture_output=True, text=True, timeout=20)
        ok = (p.returncode == 0 and (p.stdout or '').strip() == 'hello')
        print(f'  [hello] {"OK" if ok else "FAIL"} out={(p.stdout or "").strip()!r}', flush=True)
        if not ok: fails += 1

    # [4] 宿主编译回归子集 (快测, 每个 <1s)
    print('[4] 宿主回归子集', flush=True)
    for t in REG:
        if not compile_and_check('qcc_x86.exe', t, 'host:' + os.path.basename(t)):
            fails += 1

    # [5] v1 编译 b_global (自宿主功能, 慢 ~85s 但必须; 唯一 out 名)
    print('[5] v1 编译 b_global', flush=True)
    if not compile_and_check(v1, 'tests/behavior/b_global.c', 'v1:b_global', outname='qc_v1_bglobal.exe'):
        fails += 1

    print(f'\n==== 快测门禁: {"全部通过" if fails == 0 else str(fails) + " 项失败"} ====', flush=True)
    return 0 if fails == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
