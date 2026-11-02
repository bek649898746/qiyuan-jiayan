#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_hygiene —— G4 卫生判据（路线图 A1 · 2026-11-02）。

断言四件事（全部驱动真实仓库状态，不读旁证）：
  1. **仓库根零测试临时目录**：不得出现 `jyldsmoke_*` / `scratch_*` / `_check_*` /
     `_tmp` / `drectve<N>`（含被 .gitignore 忽略的目录 —— 直接 listdir，不依赖 git）。
  2. **临时根统一**：`qcc_tmp.tmp_root()` 必须解析到 `QCC_TMP_ROOT`（默认 `D:\\qcc_tmp`），
     且该目录可写；不得回落到仓库内。
  3. **生成者已止源**：`check_jyld_link_smoke.py` 必须使用 `tmp_dir(...)`，
     不得再出现 `jyldsmoke_%d` 这类「仓库根 + pid」的裸路径。
  4. **未跟踪条目受控**：`git status --porcelain` 的顶层条目逐项列出；禁止模式之外
     允许存在（受控清单），但必须先修掉禁止模式。

用法: python scripts/check_hygiene.py [--list-limit N]
退出码: 0 = PASS / 1 = RED
"""
import argparse
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

from qcc_tmp import tmp_root  # noqa: E402

BAD = re.compile(r'^(jyldsmoke_\d+|scratch_.+|_check_.+|_tmp|drectve\d+)$')


def git_untracked_tops():
    r = subprocess.run(['git', 'status', '--porcelain', '-uall'], cwd=ROOT,
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    tops = set()
    for line in (r.stdout or '').splitlines():
        if len(line) < 4:
            continue
        path = line[3:].strip()
        if path.startswith('"') and path.endswith('"'):
            path = path[1:-1]
        tops.add(path.replace('\\', '/').split('/')[0])
    return sorted(tops), r.returncode


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--list-limit', type=int, default=20)
    a = ap.parse_args()
    bad = []

    # 1) 仓库根临时目录（直接看盘，含 ignored）
    hits = [t for t in sorted(os.listdir(ROOT)) if BAD.match(t)]
    for h in hits:
        print('[RED] 仓库根存在测试临时目录: %s' % h)
    if hits:
        bad.append('root-temp-dirs=%d' % len(hits))

    # 2) 临时根
    try:
        tr = str(tmp_root())
    except Exception as e:  # pragma: no cover
        tr = ''
        print('[RED] qcc_tmp.tmp_root() 失败: %r' % (e,))
        bad.append('tmp_root-error')
    if tr:
        print('[INFO] QCC_TMP_ROOT = %s' % tr)
        if os.path.abspath(tr).lower().startswith(os.path.abspath(ROOT).lower()):
            print('[RED] 临时根落在仓库内 ⟹ 会再次污染工作区')
            bad.append('tmp_root-inside-repo')
        try:
            os.makedirs(tr, exist_ok=True)
            probe = os.path.join(tr, '.qcchyg')
            with open(probe, 'w') as f:
                f.write('ok')
            os.remove(probe)
            print('[PASS] 临时根可写')
        except OSError as e:
            print('[RED] 临时根不可写: %r' % (e,))
            bad.append('tmp_root-unwritable')

    # 3) 生成者止源（结构断言）
    smoke = os.path.join(HERE, 'check_jyld_link_smoke.py')
    try:
        s = open(smoke, encoding='utf-8', errors='replace').read()
    except OSError:
        s = ''
    if 'tmp_dir(' in s and 'jyldsmoke_%d' not in s:
        print('[PASS] check_jyld_link_smoke.py 已走 tmp_dir，仓库根无裸路径')
    else:
        print('[RED] check_jyld_link_smoke.py 仍可能在仓库根建目录')
        bad.append('smoke-not-converted')

    # 4) 未跟踪顶层清单（受控）
    tops, rc = git_untracked_tops()
    print('[INFO] 未跟踪顶层条目 %d 个（前 %d）：%s' % (len(tops), a.list_limit, tops[:a.list_limit]))
    if rc != 0:
        print('[RED] git status 失败 rc=%d' % rc)
        bad.append('git-status')

    print('== 卫生判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
