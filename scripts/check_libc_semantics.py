#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_libc_semantics —— B1：sprintf / sscanf / 字符串缓冲族 的 C 语义差分判据。

判据（全部驱动 shipped 编译器 + gcc oracle）：
  1. `_audit_probe/libc_semantics.c` 在甲言与 `gcc -O0 -w` 下均 rc=0；
  2. 两边输出**逐行一致**（过滤空行），且 10 项 `[PASS]` 都在；
  3. `--neg`：`_audit_probe/libc_semantics_neg.c` 故意断言错误，两边都必须 rc≠0
     （证明判据「该红时必须红」，不是永绿）。

用法: python scripts/check_libc_semantics.py [--cc qcc_zh_seed.exe] [--neg]
退出码: 0 = PASS / 1 = RED
"""
import argparse
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROBE = os.path.join(REPO, '_audit_probe', 'libc_semantics.c')
NEG = os.path.join(REPO, '_audit_probe', 'libc_semantics_neg.c')

TAGS = ('S1-sprintf', 'S2-sprintf-guard', 'S3-sscanf', 'S4-sscanf-partial',
        'S5-sscanf-fail', 'S6-sscanf-width', 'S7-sscanf-float', 'S8-sscanf-n',
        'S9-strcat', 'S10-strncpy')


def run(args, cwd):
    e = dict(os.environ)
    e.pop('QCC_RT_PATH', None)
    r = subprocess.run(args, cwd=cwd, capture_output=True, env=e)
    return r.returncode, (r.stdout or b'').decode('utf-8', 'replace'), (r.stderr or b'').decode('utf-8', 'replace')


def lines(out):
    return [l.rstrip() for l in out.splitlines() if l.strip()]


def neg_mode(qcc, w):
    bad = []
    for tag, cmd in (('qcc', [qcc, NEG, '-o', 'neg_qcc.exe']),
                     ('gcc', ['gcc', '-O0', '-w', NEG, '-o', 'neg_gcc.exe'])):
        rc, out, err = run(cmd, w)
        if rc != 0:
            print('[FAIL] 负例编译失败(%s) rc=%d %r' % (tag, rc, err[:160]))
            bad.append('neg-compile-' + tag)
            continue
        rc, out, err = run([os.path.join(w, 'neg_qcc.exe' if tag == 'qcc' else 'neg_gcc.exe')], w)
        ok = rc != 0
        print('%s 负例(%s) rc=%d（应为非 0）' % ('PASS' if ok else 'FAIL', tag, rc))
        if not ok:
            bad.append('neg-green-' + tag)
    print('== libc 语义负例: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    ap.add_argument('--neg', action='store_true')
    a = ap.parse_args()
    qcc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tmp_dir('qcc_libcsem')

    if a.neg:
        rc = neg_mode(qcc, w)
        shutil.rmtree(w, ignore_errors=True)
        return rc

    bad = []
    rc, out, err = run([qcc, PROBE, '-o', 'qcc.exe'], w)
    if rc != 0:
        print('[RED] 甲言编译探针失败 rc=%d %r' % (rc, err[:300]))
        bad.append('qcc-compile')
    rc, out, err = run(['gcc', '-O0', '-w', PROBE, '-o', 'gcc.exe'], w)
    if rc != 0:
        print('[RED] gcc oracle 编译失败 rc=%d %r' % (rc, err[:300]))
        bad.append('gcc-compile')

    if not bad:
        rca, outa, _ = run([os.path.join(w, 'qcc.exe')], w)
        rco, outo, _ = run([os.path.join(w, 'gcc.exe')], w)
        la, lo = lines(outa), lines(outo)
        same = la == lo
        print('%s 逐行一致 | qcc=%d gcc=%d' % ('PASS' if same else 'FAIL', len(la), len(lo)))
        if not same:
            bad.append('lines')
            for x, y in zip(la, lo):
                if x != y:
                    print('  first diff:\n   qcc : %s\n   gcc : %s' % (x, y))
                    break
        ok = rca == 0 and rco == 0
        print('%s 两边 rc=0 | qcc=%d gcc=%d' % ('PASS' if ok else 'FAIL', rca, rco))
        if not ok:
            bad.append('rc')
        for t in TAGS:
            hit = ('[PASS] %s' % t) in outa
            print('%s %s' % ('PASS' if hit else 'FAIL', t))
            if not hit:
                bad.append(t)

    shutil.rmtree(w, ignore_errors=True)
    print('== libc 语义判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
