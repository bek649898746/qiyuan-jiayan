#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_split_equiv —— C2 拆分等价判据（产物逐字节 + 自举级对拍）。

断言：
  1. `gen_unity.py` 可复跑，`build/qcc_unity.jy` 行数 = 原文件行数 + 块数（#line 插入）；
  2. seed 编单文件与 unity 的产物 **SHA256 相同**；
  3. 自举级对拍：seed 产物再分别编单文件 / unity，两份产物 SHA256 相同。

用法: python scripts/check_split_equiv.py [--cc qcc_zh_seed.exe]
退出码: 0 = PASS / 1 = RED
"""
import argparse
import hashlib
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, 'srclib_jiayan', 'qcc_zh.jy')
UNITY = os.path.join(REPO, 'build', 'qcc_unity.jy')


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for c in iter(lambda: f.read(1 << 20), b''):
            h.update(c)
    return h.hexdigest()


def run(args, cwd, timeout=900, env=None):
    e = dict(os.environ)
    if env:
        e.update(env)
    r = subprocess.run(args, cwd=cwd, capture_output=True, timeout=timeout, env=e)
    return r.returncode, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tmp_dir('qcc_split')
    bad = []

    rc, out = run([sys.executable, os.path.join(REPO, 'scripts', 'gen_unity.py')], REPO)
    if rc != 0:
        print('[RED] gen_unity 失败 rc=%d %r' % (rc, out[-200:]))
        bad.append('gen')
    else:
        n_src = open(SRC, encoding='utf-8', errors='replace').read().count('\n')
        n_u = open(UNITY, encoding='utf-8', errors='replace').read().count('\n')
        want = n_src
        ok = n_u == want
        print('%s unity 行数 %d = 原 %d（纯拼接，无 #line）' % ('PASS' if ok else 'FAIL', n_u, n_src))
        if not ok:
            bad.append('lines')

    if not bad:
        rc, out = run([cc, SRC, '-o', 'a.exe'], w)
        if rc != 0:
            bad.append('compile-src')
            print('[RED] 单文件编译 rc=%d %r' % (rc, out[-200:]))
        rc, out = run([cc, UNITY, '-o', 'b.exe'], w)
        if rc != 0:
            bad.append('compile-unity')
            print('[RED] unity 编译 rc=%d %r' % (rc, out[-200:]))
        if not bad:
            ha, hb = sha(os.path.join(w, 'a.exe')), sha(os.path.join(w, 'b.exe'))
            ok = ha == hb
            print('%s 产物逐字节 | %s vs %s' % ('PASS' if ok else 'FAIL', ha[:12], hb[:12]))
            if not ok:
                bad.append('product')
            # 自举级对拍：用 a.exe 再编两者（a.exe 在临时目录 ⟹ 显式指定运行库）
            rt = os.path.join(REPO, 'srclib', 'qcc_rt.c')
            rc, out = run([os.path.join(w, 'a.exe'), SRC, '-o', 'a2.exe'], w, env={'QCC_RT_PATH': rt})
            rc2, out2 = run([os.path.join(w, 'a.exe'), UNITY, '-o', 'b2.exe'], w, env={'QCC_RT_PATH': rt})
            if rc != 0 or rc2 != 0:
                bad.append('self-bootstrap')
                print('[RED] 自举级编译 rc=%d/%d' % (rc, rc2))
            else:
                h2a, h2b = sha(os.path.join(w, 'a2.exe')), sha(os.path.join(w, 'b2.exe'))
                ok2 = h2a == h2b
                print('%s 自举级对拍 | %s vs %s' % ('PASS' if ok2 else 'FAIL', h2a[:12], h2b[:12]))
                if not ok2:
                    bad.append('self-product')

    shutil.rmtree(w, ignore_errors=True)
    print('== 拆分等价判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
