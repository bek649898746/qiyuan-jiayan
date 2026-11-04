# -*- coding: utf-8 -*-
"""clean_tmp —— 统一清理测试临时目录（ORD-20261102-90）。

为什么存在：测试脚本的落点已统一到 `QCC_TMP_ROOT`（默认 `D:\\qcc_tmp`），但「只落不清」
仍会让 D 盘/`%TEMP%` 无限增长。本脚本只清**测试前缀**目录，带保护清单，默认 3 天前。

用法:
  python scripts/clean_tmp.py                 # 清 3 天前的测试目录
  python scripts/clean_tmp.py --days 1        # 清 1 天前
  python scripts/clean_tmp.py --dry-run       # 只报告，不删
  python scripts/clean_tmp.py --root D:\\x     # 只清指定根（默认 QCC_TMP_ROOT + %TEMP%）

保护清单（永不删除，即使命中前缀）:
  `_pub_wt` / `_realpub` / `_pub_bak_*` —— publish worktree 与备份。
"""
import argparse
import os
import shutil
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_root  # noqa: E402

PREFIXES = ('qcc_', 'dev_', 'check_', '_check_')
PROTECT = ('_pub_wt', '_realpub', '_pub_bak_')


def is_protected(name):
    return any(name == p or name.startswith(p) for p in PROTECT)


def scan(root, days):
    out = []
    if not os.path.isdir(root):
        return out
    now = time.time()
    for name in os.listdir(root):
        if not name.startswith(PREFIXES) or is_protected(name):
            continue
        p = os.path.join(root, name)
        if not os.path.isdir(p):
            continue
        age = (now - os.path.getmtime(p)) / 86400.0
        if age >= days:
            size = 0
            for dp, _dn, fn in os.walk(p):
                for f in fn:
                    try:
                        size += os.path.getsize(os.path.join(dp, f))
                    except OSError:
                        pass
            out.append((p, age, size))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--days', type=int, default=3)
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--root', action='append', default=None)
    a = ap.parse_args()

    roots = a.root or [tmp_root(), tempfile.gettempdir()]
    # 去重（QCC_TMP_ROOT 可能就指向 %TEMP% 的子目录）
    seen, uniq = set(), []
    for r in roots:
        r = os.path.abspath(r)
        if r not in seen:
            seen.add(r)
            uniq.append(r)

    print('清理根: %s' % '; '.join(uniq))
    print('保护清单（永不删）: %s' % ', '.join(PROTECT))
    total_n = total_b = 0
    for root in uniq:
        victims = scan(root, a.days)
        print('[%s] %d 个目录（>= %d 天）' % (root, len(victims), a.days))
        for p, age, size in victims:
            print('  %s  %.1f 天  %.1f MB' % (p, age, size / 1048576.0))
            total_n += 1
            total_b += size
            if not a.dry_run:
                try:
                    shutil.rmtree(p, ignore_errors=True)
                except Exception as e:  # noqa: BLE001
                    print('    [WARN] 删除失败: %s' % e)
    print('SUMMARY: %s 目录=%d 释放=%.1f MB' %
          ('DRY-RUN' if a.dry_run else 'DELETED', total_n, total_b / 1048576.0))
    return 0


if __name__ == '__main__':
    sys.exit(main())
