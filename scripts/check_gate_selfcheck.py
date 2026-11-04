#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_gate_selfcheck —— G6 判据自检（路线图 B2）。

把「判据自己错了」变成一道可复跑的门：
  [oracle] 对照类判据的 gcc 侧必须先全绿：正例 rc=0、负例 rc=1；
  [neg]    关键判据必须「该红时红」：libc 负例、snprintf 负例、include 负例；
  [gate]   卫生判据本身必须 rc=0；
  [ledger] 结果写 build/gate_selfcheck.json（时间 + 每项结论，变更留痕）。

用法: python scripts/check_gate_selfcheck.py [--json build/gate_selfcheck.json]
退出码: 0 = PASS / 1 = RED
"""
import argparse
import json
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROBE = os.path.join(REPO, '_audit_probe')


def run(args, cwd, timeout=600):
    r = subprocess.run(args, cwd=cwd, capture_output=True, timeout=timeout)
    return r.returncode, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--json', default=os.path.join(REPO, 'build', 'gate_selfcheck.json'))
    a = ap.parse_args()
    w = tmp_dir('qcc_gateself')
    bad = []
    rows = []

    def check(name, ok, detail):
        rows.append({'name': name, 'ok': bool(ok), 'detail': detail[:200]})
        print('%s %-28s %s' % ('PASS' if ok else 'FAIL', name, detail[:120]))
        if not ok:
            bad.append(name)

    # [oracle] gcc 侧：正例绿、负例红
    rc, out = run(['gcc', '-O0', '-w', os.path.join(PROBE, 'libc_semantics.c'), '-o', 'oracle_pos.exe'], w)
    if rc == 0:
        rc2, _ = run([os.path.join(w, 'oracle_pos.exe')], w)
        check('oracle-libc-pos', rc2 == 0, 'gcc 正例运行 rc=%d' % rc2)
    else:
        check('oracle-libc-pos', False, 'gcc 编译失败 rc=%d' % rc)
    rc, out = run(['gcc', '-O0', '-w', os.path.join(PROBE, 'libc_semantics_neg.c'), '-o', 'oracle_neg.exe'], w)
    if rc == 0:
        rc2, _ = run([os.path.join(w, 'oracle_neg.exe')], w)
        check('oracle-libc-neg', rc2 != 0, 'gcc 负例运行 rc=%d（应非 0）' % rc2)
    else:
        check('oracle-libc-neg', False, 'gcc 编译失败 rc=%d' % rc)

    # [neg] 关键判据的负例必须红
    rcl, _ = run([sys.executable, os.path.join(REPO, 'scripts', 'check_libc_semantics.py'), '--neg'], REPO)
    check('neg-libc-suite', rcl == 0, 'check_libc_semantics.py --neg rc=%d' % rcl)

    rc, out = run([os.path.join(REPO, 'qcc_zh_seed.exe'), os.path.join(PROBE, 'snprintf_neg.c'), '-o', 'sn_neg.exe'], w)
    if rc == 0:
        rc2, _ = run([os.path.join(w, 'sn_neg.exe')], w)
        check('neg-snprintf', rc2 != 0, 'qcc 负例运行 rc=%d（应非 0）' % rc2)
    else:
        check('neg-snprintf', False, 'qcc 编译失败 rc=%d' % rc)

    # [neg] include fail-closed
    with open(os.path.join(w, 'inc_neg.jy'), 'w', encoding='utf-8') as f:
        f.write('#include <nope_g6.jy>\nint main(void){ return 0; }\n')
    rc, out = run([os.path.join(REPO, 'qcc_zh_seed.exe'), 'inc_neg.jy', '-o', 'inc_neg.exe'], w)
    check('neg-include', rc != 0, 'qcc <nope_g6.jy> rc=%d（应非 0）' % rc)

    # [gate] 卫生判据本身
    rc, out = run([sys.executable, os.path.join(REPO, 'scripts', 'check_hygiene.py')], REPO)
    check('gate-hygiene', rc == 0, 'check_hygiene rc=%d' % rc)

    rec = {'generated_at': time.strftime('%Y-%m-%d %H:%M:%S'), 'rows': rows}
    try:
        os.makedirs(os.path.dirname(a.json), exist_ok=True)
        with open(a.json, 'w', encoding='utf-8') as f:
            json.dump(rec, f, ensure_ascii=False, indent=2)
        print('[INFO] 留痕 %s' % a.json)
    except OSError as e:
        print('[WARN] 留痕失败: %r' % (e,))

    print('== 判据自检: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
