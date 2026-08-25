# -*- coding: utf-8 -*-
"""H1==H2 现编现验 (大哥定稿 2026-08-25): 严禁用残留 exe
流程:
  1. gcc 现编 srclib/qcc_x86.c → H1 (宿主编译器)
  2. H1 编 qcc_work.jy → H1_prod (C 宿主链第一代 = v1g)
  3. H1_prod 编镜像 → v2 → v3 → v4 → v5 (收敛链)
  4. 断言 H1_prod == v2 == v3 == v4 == v5 (H1==H2: H1 == 收敛不动点)
哈希: SHA256 (与 build.ps1 一致); expected 对账
"""
import subprocess, os, sys, hashlib

os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
os.makedirs('scratch_test', exist_ok=True)

SRC = 'srclib/qcc_x86.c'
MIRROR = 'srclib_jiayan/qcc_work.jy'
H1 = os.path.join('scratch_test', '_h1h2_h1.exe')

def sha256(path):
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()[:8]

def main():
    # 1. gcc 现编 H1
    r = subprocess.run(['gcc', '-O2', '-Wall', '-Werror', SRC, '-o', H1], capture_output=True, timeout=180)
    if r.returncode != 0:
        print('[FAIL] gcc 现编失败:', r.stderr.decode('utf-8', errors='replace')[-300:])
        return 1
    print('[1] gcc 现编 H1 OK')

    # 2. H1 编镜像 → H1_prod
    p1 = os.path.join('scratch_test', '_h1h2_v1.exe')
    r = subprocess.run([H1, MIRROR, '-o', p1], capture_output=True, timeout=120)
    if r.returncode != 0:
        print('[FAIL] H1 编镜像失败:', r.stderr.decode('utf-8', errors='replace')[-300:])
        return 1
    h1 = sha256(p1)
    print('[2] H1_prod (C宿主链第一代) SHA256 =', h1)

    # 3. 推收敛链 v2..v5
    cur = p1
    hs = [h1]
    for i in range(2, 6):
        nxt = os.path.join('scratch_test', '_h1h2_v%d.exe' % i)
        r = subprocess.run([cur, MIRROR, '-o', nxt], capture_output=True, timeout=120)
        if r.returncode != 0:
            print('[FAIL] v%d 编镜像失败' % i)
            return 1
        hs.append(sha256(nxt))
        cur = nxt
    print('[3] v2..v5:', hs[1:])

    # 4. H1==H2 断言
    ok = len(set(hs)) == 1
    print('[4] H1==H2 (H1==V2..V5 全等):', '✅ 通过' if ok else '❌ 失败')
    print('    H1_prod = %s, V2-V5 = %s' % (h1, hs[1]))
    # 与 build.ps1 expected 对账
    exp = None
    raw = open('scripts/build.ps1', 'rb').read().decode('gbk', errors='replace')
    for l in raw.split('\n'):
        if 'expected =' in l and chr(39) in l:
            exp = l.split(chr(39))[1].strip()
    print('    build.ps1 expected =', exp, '对账:', '✅' if exp == h1 else '❌')
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
