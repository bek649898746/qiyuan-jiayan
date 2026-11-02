# -*- coding: utf-8 -*-
"""check_stderr_contract —— 内部缺陷追踪 编译器 stderr 契约。

判据：
  1. 默认成功编译 stderr == 0 字节；
  2. `QCC_OPT=none` 同样 0；
  3. `QCC_OPT_VERBOSE=1` 恢复 `[OPT]` 统计行；
  4. 失败编译 stderr 非空且含可读诊断（A####）；
  5. 同一文件连续两次 VERBOSE 编译的折叠计数相同（单文件从 0 起算、不跨编译累计）；
  6. 发布编译器 `qcc_zh_v2.exe` 的 SHA256 前缀 == `build_zh.ps1` 的 `$expected`（自举收敛证据）。

用法: python scripts/check_stderr_contract.py [--cc qcc_zh_v1.exe]
退出码: 0 = PASS / 1 = RED
"""
import argparse
import hashlib
import io
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def run(args, cwd, env=None):
    e = dict(os.environ)
    if env:
        e.update(env)
    r = subprocess.run(args, cwd=cwd, capture_output=True, env=e)
    return r.returncode, (r.stdout or b'').decode('utf-8', 'replace'), (r.stderr or b'').decode('utf-8', 'replace')


def expected_hash():
    p = os.path.join(REPO, 'scripts', 'build_zh.ps1')
    t = io.open(p, encoding='utf-8', errors='replace').read()
    m = re.search(r"^\$expected\s*=\s*'([0-9A-Fa-f]{8})'", t, re.M)
    return m.group(1).upper() if m else ''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    qcc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tmp_dir('qcc_stderr')
    bad = []

    def wfile(name, text):
        p = os.path.join(w, name)
        io.open(p, 'w', encoding='utf-8', newline='\n').write(text)
        return p

    hello = wfile('hello.jy', '整 主(空) { 输出("hi\\n"); 返 0; }\n')
    rc, out, err = run([qcc, hello, '-o', 'hello.exe'], w)
    ok = rc == 0 and err == ''
    print('%s 1 默认成功 stderr==0 | rc=%d len=%d' % ('PASS' if ok else 'FAIL', rc, len(err)))
    if not ok:
        bad.append('1')
    rc, out, err = run([qcc, hello, '-o', 'hello2.exe'], w, {'QCC_OPT': 'none'})
    ok = rc == 0 and err == ''
    print('%s 2 QCC_OPT=none stderr==0 | rc=%d len=%d' % ('PASS' if ok else 'FAIL', rc, len(err)))
    if not ok:
        bad.append('2')
    rc, out, err = run([qcc, hello, '-o', 'hello3.exe'], w, {'QCC_OPT_VERBOSE': '1'})
    ok = rc == 0 and '[OPT]' in err
    print('%s 3 VERBOSE 恢复 [OPT] | rc=%d %r' % ('PASS' if ok else 'FAIL', rc, err[:100]))
    if not ok:
        bad.append('3')

    bad_src = wfile('bad.jy', '整 主(空) { 整 x = 1 返 0; }\n')
    rc, out, err = run([qcc, bad_src, '-o', 'bad.exe'], w)
    ok = rc != 0 and len(err) > 0 and re.search(r'A\d{4}', err) is not None
    print('%s 4 失败 stderr 非空且可读 | rc=%d %r' % ('PASS' if ok else 'FAIL', rc, err[:140]))
    if not ok:
        bad.append('4')

    rc1, _, e1 = run([qcc, hello, '-o', 'c1.exe'], w, {'QCC_OPT_VERBOSE': '1'})
    rc2, _, e2 = run([qcc, hello, '-o', 'c2.exe'], w, {'QCC_OPT_VERBOSE': '1'})
    m1 = re.search(r'改\s*(\d+)\s*处', e1)
    m2 = re.search(r'改\s*(\d+)\s*处', e2)
    ok = bool(m1 and m2) and m1.group(1) == m2.group(1)
    print('%s 5 本文件计数不累计 | %s / %s' % ('PASS' if ok else 'FAIL',
          m1.group(1) if m1 else '?', m2.group(1) if m2 else '?'))
    if not ok:
        bad.append('5')

    exp = expected_hash()
    v2 = os.path.join(REPO, 'qcc_zh_v2.exe')
    got = hashlib.sha256(io.open(v2, 'rb').read()).hexdigest()[:8].upper() if os.path.exists(v2) else ''
    ok = bool(exp) and got == exp
    print('%s 6 自举收敛 v2==$expected | expected=%s v2=%s' % ('PASS' if ok else 'FAIL', exp, got or '<missing>'))
    if not ok:
        bad.append('6')

    shutil.rmtree(w, ignore_errors=True)
    print('== stderr 契约判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
