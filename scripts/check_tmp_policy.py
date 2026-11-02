# -*- coding: utf-8 -*-
"""check_tmp_policy —— 内部缺陷追踪 测试仓位统一与清理判据。

判据：
  1. 所有测试脚本的 TEMP/mkdtemp/gettempdir 写法已收敛（仅入口/清理脚本自身除外）；
  2. `QCC_TMP_ROOT` 可重定向：`tmp_dir()` 产物落在指定根下；
  3. `clean_tmp.py --dry-run` 保护清单含 `_pub_wt` / `_realpub`，且不把它们列为待删；
  4. `--days 3` 能实删 4 天前的 `qcc_*`，保留新目录与保护目录。

用法: python scripts/check_tmp_policy.py
退出码: 0 = PASS / 1 = RED
"""
import glob
import io
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPTS = os.path.join(REPO, 'scripts')
EXCLUDE = {'qcc_tmp.py', 'clean_tmp.py', 'check_tmp_policy.py',
           # 历史判据 §四.4「判据不动」冻结该文件（`check_gd52_row.py` 硬查 git diff 为空）；
           # 它仅在性能测量时用一次 %TEMP%，不常驻。属**声明例外**，不是漏改。
           'check_compile_speed.py'}
PAT = re.compile(r"environ\.get\(['\"]TEMP|mkdtemp|gettempdir")


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('--scan-dir', default=SCRIPTS,
                    help='被扫描的脚本目录（修前回执可指向 git show 导出的旧目录）')
    a = ap.parse_args()
    scan_dir = a.scan_dir
    bad = []
    fs = glob.glob(os.path.join(scan_dir, '*.py')) + glob.glob(os.path.join(scan_dir, '*.ps1'))
    left = []
    cover = 0
    for f in fs:
        name = os.path.basename(f)
        t = io.open(f, encoding='utf-8', errors='replace').read()
        if 'qcc_tmp' in t or 'QCC_TMP_ROOT' in t:
            cover += 1
        if name in EXCLUDE:
            continue
        for i, l in enumerate(t.splitlines(), 1):
            if PAT.search(l):
                left.append('%s:%d' % (name, i))
    ok = not left and cover >= 134
    print('%s 1 仓位收敛 | 剩余写点=%d 覆盖脚本=%d（期望 >=134）' % ('PASS' if ok else 'FAIL', len(left), cover))
    if left:
        print('    剩余: %s' % ', '.join(left[:10]))
    if not ok:
        bad.append('1')

    root = tmp_dir('qcc_tmp_policy_root')
    os.environ['QCC_TMP_ROOT'] = root
    d = tmp_dir('qcc_probe')
    ok = os.path.abspath(d).startswith(os.path.abspath(root) + os.sep)
    print('%s 2 QCC_TMP_ROOT 可重定向 | %s' % ('PASS' if ok else 'FAIL', d))
    if not ok:
        bad.append('2')

    test_root = tmp_dir('qcc_tmp_policy_clean')
    for n in ('qcc_old', 'qcc_new', '_pub_wt', '_realpub'):
        os.makedirs(os.path.join(test_root, n), exist_ok=True)
    old = time.time() - 4 * 86400
    for n in ('qcc_old', '_pub_wt', '_realpub'):
        os.utime(os.path.join(test_root, n), (old, old))
    clean = os.path.join(SCRIPTS, 'clean_tmp.py')
    r = subprocess.run([sys.executable, clean, '--root', test_root, '--days', '3', '--dry-run'],
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    out = (r.stdout or '') + (r.stderr or '')
    ok_protect = '_pub_wt' in out and '_realpub' in out and 'qcc_old' in out
    ok_not_listed = not re.search(r'^\s+.*(_pub_wt|_realpub)\s+.*天', out, re.M)
    ok = r.returncode == 0 and ok_protect and ok_not_listed
    print('%s 3 清理保护清单 | rc=%d 保护项在/未待删=%s' % ('PASS' if ok else 'FAIL', r.returncode, ok_not_listed))
    if not ok:
        bad.append('3')

    r = subprocess.run([sys.executable, clean, '--root', test_root, '--days', '3'],
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    remain = sorted(os.listdir(test_root))
    ok = (not os.path.exists(os.path.join(test_root, 'qcc_old'))
          and os.path.exists(os.path.join(test_root, 'qcc_new'))
          and os.path.exists(os.path.join(test_root, '_pub_wt'))
          and os.path.exists(os.path.join(test_root, '_realpub')))
    print('%s 4 3 天前实删、保护项保留 | remain=%s' % ('PASS' if ok else 'FAIL', remain))
    if not ok:
        bad.append('4')

    # 5 运行时落点：设 QCC_TMP_ROOT，跑 dev_direct_run + 两个 check，%TEMP% 前后快照
    temp = os.environ.get('TEMP', '')
    before = set(os.listdir(temp)) if os.path.isdir(temp) else set()
    env = dict(os.environ, QCC_TMP_ROOT=root)
    r1 = subprocess.run([sys.executable, os.path.join(SCRIPTS, 'dev_direct_run.py'), 'feat_zh_2phase30_ok.c'],
                        cwd=REPO, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=300)
    r2 = subprocess.run([sys.executable, os.path.join(SCRIPTS, 'check_blockproto.py')],
                        cwd=REPO, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=300)
    r3 = subprocess.run([sys.executable, os.path.join(SCRIPTS, 'check_c_mode_smoke.py')],
                        cwd=REPO, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=300)
    after = set(os.listdir(temp)) if os.path.isdir(temp) else set()
    new = sorted(n for n in (after - before) if n.startswith(('qcc_', 'dev_', '_check_')))
    ok = r1.returncode == 0 and r2.returncode == 0 and r3.returncode == 0 and not new
    print('%s 5 运行时落点 | dev=%d blockproto=%d c_mode=%d 新增TEMP前缀=%s'
          % ('PASS' if ok else 'FAIL', r1.returncode, r2.returncode, r3.returncode, new or '无'))
    if not ok:
        bad.append('5')

    shutil.rmtree(test_root, ignore_errors=True)
    shutil.rmtree(root, ignore_errors=True)
    print('== 测试仓位判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
